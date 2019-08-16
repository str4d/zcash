// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ASYNCRPCOPERATION_ISSUEASSET_H
#define ASYNCRPCOPERATION_ISSUEASSET_H

#include "asyncrpcoperation.h"
#include "amount.h"
#include "primitives/transaction.h"
#include "transaction_builder.h"
#include "zcash/Address.hpp"
#include "wallet.h"
#include "wallet/paymentdisclosure.h"

#include <unordered_map>
#include <tuple>

#include <univalue.h>

// Default transaction fee if caller does not specify one.
#define ISSUE_ASSET_DEFAULT_MINERS_FEE   10000

using namespace libzcash;

struct IssueAssetUTXO {
    uint256 txid;
    int vout;
    CScript scriptPubKey;
    CAmount amount;
};

class AsyncRPCOperation_issueasset : public AsyncRPCOperation {
public:
    AsyncRPCOperation_issueasset(
        TransactionBuilder builder,
        std::vector<IssueAssetUTXO> inputs,
        std::string toAddress,
        uint32_t assetType,
        CAmount assetAmount,
        CAmount fee = ISSUE_ASSET_DEFAULT_MINERS_FEE,
        UniValue contextInfo = NullUniValue);
    virtual ~AsyncRPCOperation_issueasset();

    // We don't want to be copied or moved around
    AsyncRPCOperation_issueasset(AsyncRPCOperation_issueasset const&) = delete;             // Copy construct
    AsyncRPCOperation_issueasset(AsyncRPCOperation_issueasset&&) = delete;                  // Move construct
    AsyncRPCOperation_issueasset& operator=(AsyncRPCOperation_issueasset const&) = delete;  // Copy assign
    AsyncRPCOperation_issueasset& operator=(AsyncRPCOperation_issueasset &&) = delete;      // Move assign

    virtual void main();

    virtual UniValue getStatus() const;

    bool testmode = false;  // Set to true to disable sending txs and generating proofs

private:
    UniValue contextinfo_;     // optional data to include in return value from getStatus()

    CAmount fee_;
    SaplingPaymentAddress tozaddr_;
    uint32_t assetType_;
    CAmount assetAmount_;

    std::vector<IssueAssetUTXO> inputs_;

    TransactionBuilder builder_;
    CTransaction tx_;

    bool main_impl();

    void lock_utxos();

    void unlock_utxos();
};


#endif /* ASYNCRPCOPERATION_ISSUEASSET_H */

