use incrementalmerkletree::{bridgetree::BridgeTree, Frontier, Tree};
use libc::c_uchar;
use std::cmp::Ordering;
use std::collections::{BTreeMap, HashMap, HashSet};
use tracing::error;

use zcash_primitives::{
    consensus::BlockHeight,
    transaction::{components::Amount, TxId},
};

use orchard::{
    bundle::Authorized,
    keys::{FullViewingKey, IncomingViewingKey, SpendingKey},
    tree::MerkleHashOrchard,
    Address, Bundle, Note,
};

use super::incremental_merkle_tree_ffi::MERKLE_DEPTH;

pub const MAX_CHECKPOINTS: usize = 100;

#[derive(Debug, Clone)]
pub struct LastObserved {
    height: BlockHeight,
    block_tx_idx: usize,
}

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct OutPoint {
    txid: TxId,
    action_idx: usize,
}

#[derive(Debug, Clone)]
pub struct DecryptedNote {
    ivk: IncomingViewingKey,
    note: Note,
    recipient: Address,
    memo: [u8; 512],
}

struct WalletTx {
    txid: TxId,
    decrypted_notes: BTreeMap<usize, DecryptedNote>,
}

/// Internal newtype wrapper that allows us to use addresses as
/// BTreeMap keys.
#[derive(Clone, Copy, PartialEq, Eq)]
pub(crate) struct WalletAddress(Address);

impl WalletAddress {
    pub(crate) fn bytes_cmp(a0: &Address, a1: &Address) -> Ordering {
        (&a0.to_raw_address_bytes()).cmp(&a1.to_raw_address_bytes())
    }
}

impl PartialOrd for WalletAddress {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for WalletAddress {
    fn cmp(&self, other: &Self) -> Ordering {
        WalletAddress::bytes_cmp(&self.0, &other.0)
    }
}

#[derive(Debug, Clone)]
struct KeyMetadata {
    version: i32,
    create_time: u64,
    hd_key_path: Option<String>,
    seed_fingerprint: [u8; 32],
}

struct KeyStore {
    payment_addresses: BTreeMap<WalletAddress, IncomingViewingKey>,
    viewing_keys: BTreeMap<IncomingViewingKey, FullViewingKey>,
    spending_keys: BTreeMap<FullViewingKey, SpendingKey>,
}

impl KeyStore {
    pub fn empty() -> Self {
        KeyStore {
            payment_addresses: BTreeMap::new(),
            viewing_keys: BTreeMap::new(),
            spending_keys: BTreeMap::new(),
        }
    }

    pub fn add_full_viewing_key(&mut self, fvk: FullViewingKey) {
        let ivk = IncomingViewingKey::from(&fvk);
        self.viewing_keys.insert(ivk, fvk);
    }

    pub fn add_spending_key(&mut self, sk: SpendingKey) {
        let fvk = FullViewingKey::from(&sk);
        self.add_full_viewing_key(fvk.clone());
        self.spending_keys.insert(fvk, sk);
    }

    pub fn add_raw_address(&mut self, addr: Address, ivk: IncomingViewingKey) {
        self.payment_addresses.insert(WalletAddress(addr), ivk);
    }

    pub fn spending_key_for_ivk(&self, ivk: &IncomingViewingKey) -> Option<&SpendingKey> {
        self.viewing_keys
            .get(ivk)
            .and_then(|fvk| self.spending_keys.get(fvk))
    }

    pub fn spending_key_for_address(&self, addr: &Address) -> Option<&SpendingKey> {
        self.payment_addresses
            .get(&WalletAddress(*addr))
            .and_then(|ivk| self.spending_key_for_ivk(ivk))
    }
}

pub struct Wallet {
    last_observed: Option<LastObserved>,
    key_store: KeyStore,
    witness_tree: BridgeTree<MerkleHashOrchard, MERKLE_DEPTH>,
    wallet_txs: HashMap<TxId, WalletTx>,
}

#[derive(Debug, Clone)]
pub enum WalletError {
    OutOfOrder(LastObserved, BlockHeight, usize),
    NoteCommitmentTreeFull,
}

impl Wallet {
    pub fn empty() -> Self {
        Wallet {
            key_store: KeyStore::empty(),
            last_observed: None,
            witness_tree: BridgeTree::new(MAX_CHECKPOINTS),
            wallet_txs: HashMap::new(),
        }
    }

    pub fn checkpoint_witness_tree(&mut self) {
        self.witness_tree.checkpoint();
    }

    pub fn rewind_witness_tree(&mut self) -> bool {
        self.witness_tree.rewind()
    }

    /// Add note data for those notes that are decryptable with one of this wallet's
    /// incoming viewing keys to the wallet, and return the indices of the actions
    /// that we were able to decrypt.
    pub fn add_notes_from_bundle(
        &mut self,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Result<Vec<usize>, WalletError> {
        let mut wallet_tx = WalletTx {
            txid: *txid,
            decrypted_notes: BTreeMap::new(),
        };

        let keys = self
            .key_store
            .viewing_keys
            .keys()
            .cloned()
            .collect::<Vec<_>>();
        let mut result = vec![];
        for (action_idx, ivk, note, recipient, memo) in bundle.decrypt_outputs_for_keys(&keys) {
            // Mark the witness tree with the fact that we want to be able to compute
            // a witness for this note.
            let note_data = DecryptedNote {
                ivk: ivk.clone(),
                note,
                recipient,
                memo,
            };

            wallet_tx.decrypted_notes.insert(action_idx, note_data);
            self.key_store.add_raw_address(recipient, ivk);
            result.push(action_idx);
        }

        if !wallet_tx.decrypted_notes.is_empty() {
            self.wallet_txs.insert(*txid, wallet_tx);
        }

        Ok(result)
    }

    /// Add note commitments for the Orchard components of a transaction to the note commitment
    /// tree, and mark the tree at the notes decryptable by this wallet so that in the future
    /// we can produce authentication paths to those notes.
    ///
    /// * `block_height` - Height of the block containing the transaction that provided this bundle.
    /// * `block_tx_idx` - Index of the transaction within the block
    /// * `txid` - Identifier of the transaction.
    /// * `bundle` - Orchard component of the transaction.
    pub fn append_bundle_commitments(
        &mut self,
        block_height: BlockHeight,
        block_tx_idx: usize,
        txid: &TxId,
        bundle: &Bundle<Authorized, Amount>,
    ) -> Result<(), WalletError> {
        // Check that the wallet is in the correct state to update the note commitment tree with
        // new outputs.
        if !self.last_observed.iter().all(|last_observed| {
            (block_height == last_observed.height && block_tx_idx == last_observed.block_tx_idx + 1)
                || (block_height == last_observed.height + 1 && block_tx_idx == 0)
        }) {
            return Err(WalletError::OutOfOrder(
                // this unwrap is safe, because `all` above ensures that if we have not observed
                // any blocks, we cannot be observing a block out of order
                self.last_observed.as_ref().unwrap().clone(),
                block_height,
                block_tx_idx,
            ));
        }

        let wallet_tx = self.wallet_txs.get(&txid);
        for (action_idx, action) in bundle.actions().iter().enumerate() {
            if !self
                .witness_tree
                .append(&MerkleHashOrchard::from_cmx(action.cmx()))
            {
                return Err(WalletError::NoteCommitmentTreeFull);
            }

            if let Some(wallet_tx) = wallet_tx {
                if wallet_tx.decrypted_notes.contains_key(&action_idx) {
                    self.witness_tree.witness();
                }
            }
        }

        Ok(())
    }

    pub fn is_mine(&self, txid: &TxId) -> bool {
        return match self.wallet_txs.get(&txid) {
            Some(wtx) if !wtx.decrypted_notes.is_empty() => true,
            _ => false,
        };
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_new() -> *mut Wallet {
    let empty_wallet = Wallet::empty();
    Box::into_raw(Box::new(empty_wallet))
}

#[no_mangle]
pub extern "C" fn orchard_wallet_free(wallet: *mut Wallet) {
    if !wallet.is_null() {
        drop(unsafe { Box::from_raw(wallet) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_checkpoint(wallet: *mut Wallet) {
    let wallet = unsafe { &mut *wallet };
    wallet.checkpoint_witness_tree();
}

#[no_mangle]
pub extern "C" fn orchard_wallet_rewind(wallet: *mut Wallet) -> bool {
    let wallet = unsafe { &mut *wallet };
    wallet.rewind_witness_tree()
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_notes_from_bundle(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());
    match unsafe { bundle.as_ref() } {
        Some(bundle) => match wallet.add_notes_from_bundle(&txid, bundle) {
            Err(e) => {
                error!("An error occurred adding the Orchard bundle's notes to the wallet for txid {:?}: {:?}", txid, e);
                false
            }
            Ok(_) => true,
        },
        None => true,
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_append_bundle_commitments(
    wallet: *mut Wallet,
    block_height: u32,
    block_tx_idx: usize,
    txid: *const [c_uchar; 32],
    bundle: *const Bundle<Authorized, Amount>,
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());
    match unsafe { bundle.as_ref() } {
        Some(bundle) => {
            match wallet.append_bundle_commitments(block_height.into(), block_tx_idx, &txid, bundle)
            {
                Err(e) => {
                    error!("An error occurred adding the Orchard bundle's notes to the note commitment tree: {:?}", e);
                    false
                }
                Ok(()) => true,
            }
        }
        None => true,
    }
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_spending_key(wallet: *mut Wallet, sk: *const SpendingKey) {
    let wallet = unsafe { &mut *wallet };
    let sk = unsafe { &*sk };

    wallet.key_store.add_spending_key(*sk);
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_full_viewing_key(
    wallet: *mut Wallet,
    fvk: *const FullViewingKey,
) {
    let wallet = unsafe { &mut *wallet };
    let fvk = unsafe { &*fvk };

    wallet.key_store.add_full_viewing_key(fvk.clone());
}

#[no_mangle]
pub extern "C" fn orchard_wallet_add_raw_address(
    wallet: *mut Wallet,
    addr: *const Address,
    ivk: *const IncomingViewingKey,
) {
    let wallet = unsafe { &mut *wallet };
    let addr = unsafe { &*addr };
    let ivk = unsafe { &*ivk };

    wallet.key_store.add_raw_address(addr.clone(), ivk.clone());
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_is_mine(
    wallet: *mut Wallet,
    txid: *const [c_uchar; 32],
) -> bool {
    let wallet = unsafe { &mut *wallet };
    let txid = TxId::from_bytes(unsafe { &*txid }.clone());

    wallet.is_mine(&txid)
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_data_new() -> *mut Vec<usize> {
    let v = vec![];
    Box::into_raw(Box::new(v))
}

#[no_mangle]
pub extern "C" fn orchard_wallet_tx_data_free(tx_data: *mut Vec<usize>) {
    if !tx_data.is_null() {
        drop(unsafe { Box::from_raw(tx_data) });
    }
}
