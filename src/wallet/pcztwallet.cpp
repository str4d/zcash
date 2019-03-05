// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pczt.pb.h>
#include <util/bip32.h>
#include <wallet/pcztwallet.h>
#include <zcash/Address.hpp>
#include <zcash/Note.hpp>

#define DEFAULT_FEE   10000

std::string TransactionErrorString(const TransactionError err)
{
    switch (err) {
        case TransactionError::OK:
            return "No error";
        case TransactionError::MISSING_SPENDING_KEY:
            return "Missing spending key for Sapling address";
        case TransactionError::MISSING_WITNESS:
            return "Missing witness for Sapling note";
    }
    assert(false);
}

TransactionError FundPczt(
    CWallet* pwallet,
    Pczt& pczt,
    libzcash::SaplingPaymentAddress& address)
{
    LOCK(pwallet->cs_wallet);

    // Get the spending key for the given address
    // TODO: Support only storing the proving key, leaving spend authority elsewhere
    libzcash::SaplingExtendedSpendingKey extsk;
    if (!pwallet->GetSaplingExtendedSpendingKey(address, extsk)) {
        return TransactionError::MISSING_SPENDING_KEY;
    }
    auto extfvk = extsk.ToXFVK();
    auto keyMetadata = pwallet->mapSaplingZKeyMetadata[extfvk.fvk.in_viewing_key()];

    std::vector<uint32_t> keypath;
    ParseHDKeypath(keyMetadata.hdKeypath, keypath);

    pczt::Zip32Key zip32Key;
    zip32Key.set_masterfingerprint(keyMetadata.seedFp.begin(), keyMetadata.seedFp.size());
    for (auto idx : keypath) {
        zip32Key.add_derivationpath(idx);
    }

    // Select notes from the wallet
    std::vector<SaplingOutPoint> ops;
    std::vector<libzcash::SaplingNote> notes;
    {
        std::vector<SproutNoteEntry> sproutEntries;
        std::vector<SaplingNoteEntry> saplingEntries;
        std::set<libzcash::PaymentAddress> filterAddresses;
        filterAddresses.insert(address);
        pwallet->GetFilteredNotes(sproutEntries, saplingEntries, filterAddresses, 1, INT_MAX, true, false);

        // Sort in descending order, so big notes appear first
        std::sort(saplingEntries.begin(), saplingEntries.end(),
            [](SaplingNoteEntry i, SaplingNoteEntry j) -> bool {
                return i.note.value() > j.note.value();
            });

        // Extract the data we need
        for (auto entry : saplingEntries) {
            ops.push_back(entry.op);
            notes.push_back(entry.note);
        }
    }

    // Fetch Sapling anchor and witnesses
    uint256 anchor;
    std::vector<boost::optional<SaplingWitness>> witnesses;
    pwallet->GetSaplingNoteWitnesses(ops, witnesses, anchor);

    pczt.SetSaplingAnchor(anchor);

    // Add Sapling spends
    for (size_t i = 0; i < notes.size(); i++) {
        if (!witnesses[i]) {
            return TransactionError::MISSING_WITNESS;
        }
        pczt.AddSaplingSpend(zip32Key, extsk.expsk, notes[i], witnesses[i].get());
        if (pczt.GetFee() >= DEFAULT_FEE) {
            break;
        }
    }

    // Add change output
    auto change = pczt.GetFee() - DEFAULT_FEE;
    if (change > 0) {
        pczt.AddSaplingOutput(zip32Key, extsk.expsk.ovk, address, change);
    }

    // TODO: Pad outputs to minimum of two

    return TransactionError::OK;
}
