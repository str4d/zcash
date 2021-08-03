// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ORCHARD_WALLET_H
#define ZCASH_ORCHARD_WALLET_H

#include <array>

#include "primitives/transaction.h"
#include "rust/orchard/keys.h"
#include "rust/orchard/wallet.h"
#include "zcash/address/orchard.hpp"

class OrchardNoteMetadata
{
private:
    OrchardOutPoint op;
    libzcash::OrchardRawAddress address;
    CAmount noteValue;
    std::array<uint8_t, ZC_MEMO_SIZE> memo;
    int confirmations;
public:
    OrchardNoteMetadata(
        OrchardOutPoint op,
        const libzcash::OrchardRawAddress& address,
        CAmount noteValue,
        const std::array<unsigned char, ZC_MEMO_SIZE>& memo):
        op(op), address(address), noteValue(noteValue), memo(memo), confirmations(0) {}

    const OrchardOutPoint& GetOutPoint() const {
        return op;
    }

    void SetConfirmations(int c) {
        confirmations = c;
    }

    int GetConfirmations() const {
        return confirmations;
    }

    CAmount GetNoteValue() const {
        return noteValue;
    }
};

class OrchardWallet
{
private:
    std::unique_ptr<OrchardWalletPtr, decltype(&orchard_wallet_free)> inner;

public:
    OrchardWallet() : inner(orchard_wallet_new(), orchard_wallet_free) {}

    OrchardWallet(OrchardWallet&& wallet_data) : inner(std::move(wallet_data.inner)) {}

    OrchardWallet& operator=(OrchardWallet&& wallet)
    {
        if (this != &wallet) {
            inner = std::move(wallet.inner);
        }
        return *this;
    }

    void CheckpointNoteCommitmentTree() {
        orchard_wallet_checkpoint(inner.get());
    }

    bool RewindToLastCheckpoint() {
        return orchard_wallet_rewind(inner.get());
    }

    bool AddNotes(const CTransaction& tx) {
        return orchard_wallet_add_notes_from_bundle(
                inner.get(),
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner.get());
    }

    bool AppendNoteCommitments(const int nHeight, const size_t blockTxIdx, const CTransaction& tx) {
        return orchard_wallet_append_bundle_commitments(
                inner.get(),
                (uint32_t) nHeight,
                blockTxIdx,
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner.get()
                );
    }

    bool IsMine(const uint256& txid) {
        return orchard_wallet_tx_is_mine(
                inner.get(),
                txid.begin());
    }

    void AddSpendingKey(const libzcash::OrchardSpendingKey& sk) {
        orchard_wallet_add_spending_key(inner.get(), sk.inner.get());
    }

    void AddFullViewingKey(const libzcash::OrchardFullViewingKey& fvk) {
        orchard_wallet_add_full_viewing_key(inner.get(), fvk.inner.get());
    }

    void AddRawAddress(
            const libzcash::OrchardRawAddress& addr,
            const libzcash::OrchardIncomingViewingKey& ivk) {
        orchard_wallet_add_raw_address(inner.get(), addr.inner.get(), ivk.inner.get());
    }

    static void PushOrchardNoteMeta(void* orchardNotesRet, RawOrchardNoteMetadata rawNoteMeta) {
        uint256 txid;
        std::move(std::begin(rawNoteMeta.txid), std::end(rawNoteMeta.txid), txid.begin());
        OrchardOutPoint op(txid, rawNoteMeta.actionIdx);
        // TODO: what's the efficient way to copy the memo in the OrchardNoteMetadata
        // constructor?
        std::array<uint8_t, ZC_MEMO_SIZE> memo;
        std::move(std::begin(rawNoteMeta.memo), std::end(rawNoteMeta.memo), memo.begin());
        OrchardNoteMetadata noteMeta(
                op,
                libzcash::OrchardRawAddress(rawNoteMeta.addr),
                rawNoteMeta.noteValue,
                memo);
        // TODO: noteMeta.confirmations is only available from the C++ wallet

        reinterpret_cast<std::vector<OrchardNoteMetadata>*>(orchardNotesRet)->push_back(noteMeta);
    }

    void GetFilteredNotes(
        std::vector<OrchardNoteMetadata>& orchardNotesRet,
        const std::optional<std::set<libzcash::OrchardRawAddress>> addrs,
        bool ignoreSpent,
        bool requireSpendingKey) const {

        std::vector<OrchardRawAddressPtr*> addr_ptrs;
        if (addrs.has_value()) {
            std::transform(
                    addrs.value().begin(), addrs.value().end(), std::back_inserter(addr_ptrs),
                    [](const libzcash::OrchardRawAddress& addr) {
                        return addr.inner.get();
                    });
        }

        orchard_wallet_get_filtered_notes(
            inner.get(),
            addrs.has_value(),
            addr_ptrs.data(),
            addr_ptrs.size(),
            ignoreSpent,
            requireSpendingKey,
            &orchardNotesRet,
            PushOrchardNoteMeta
            );
    }
};

#endif // ZCASH_ORCHARD_WALLET_H
