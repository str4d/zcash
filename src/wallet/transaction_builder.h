// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRANSACTION_BUILDER_H
#define TRANSACTION_BUILDER_H

#include "amount.h"
#include "zcash/Note.hpp"

#include <univalue.h>

using namespace libzcash;

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

    UniValue sign_send_raw_transaction(UniValue obj); // throws exception if there was an error

    void SavePaymentDisclosureData();
};

#endif /* TRANSACTION_BUILDER_H */
