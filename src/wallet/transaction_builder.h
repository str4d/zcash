// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRANSACTION_BUILDER_H
#define TRANSACTION_BUILDER_H

#include "amount.h"
#include "primitives/transaction.h"
#include "wallet/wallet.h"
#include "zcash/Note.hpp"

#include <univalue.h>

using namespace libzcash;

// Input UTXO is a tuple of (txid, vout), amount, coinbase
typedef std::tuple<COutPoint, CAmount, bool> TransparentInput;

// Input JSOP is a tuple of JSOutpoint, note and amount
typedef std::tuple<JSOutPoint, Note, CAmount> ShieldedInput;

// A recipient is a tuple of address, amount, memo (optional if zaddr)
typedef std::tuple<std::string, CAmount, std::string> Recipient;

// Package of info which is passed to perform_joinsplit methods.
struct JoinSplitInfo {
    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<Note> notes;
    CAmount vpub_old = 0;
    CAmount vpub_new = 0;
};

class TransactionBuilder
{
private:
    bool testmode;

    uint256 joinSplitPubKey_;
    unsigned char joinSplitPrivKey_[crypto_sign_SECRETKEYBYTES];

    std::vector<TransparentInput> t_inputs_;
    std::vector<ShieldedInput> z_inputs_;
    std::vector<Recipient> t_outputs_;
    std::vector<Recipient> z_outputs_;

    CTransaction tx_;

    // payment disclosure!
    std::vector<PaymentDisclosureKeyInfo> paymentDisclosureData_;

public:
    void TransactionBuilder::PrepareForShielded();

    // JoinSplit without any input notes to spend
    UniValue perform_joinsplit(JoinSplitInfo&);

    // JoinSplit with input notes to spend (JSOutPoints))
    UniValue perform_joinsplit(JoinSplitInfo&, std::vector<JSOutPoint>&);

    // JoinSplit where you have the witnesses and anchor
    UniValue perform_joinsplit(
        JoinSplitInfo& info,
        std::vector<boost::optional<ZCIncrementalWitness>> witnesses,
        uint256 anchor);

public:
    void AddTransparentInput(COutPoint prevout, CAmount value, bool coinbase, uint32_t nSequence);

    void AddTransparentOutput(CTxOut txo);

    void AddShieldedInput(JSOutPoint jsop, Note note);

    void AddShieldedOutput();

    void FillTransparentInputs();

    void FillShieldedInputs();

    void SetAnchor();

    void GetProofs();

    void SignTransparent(); // throws exception if there was an error

    void SignShielded();

    UniValue Send(); // throws exception if there was an error

    void SavePaymentDisclosureData();
};

#endif /* TRANSACTION_BUILDER_H */
