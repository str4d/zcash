// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "asyncrpcoperation_issueasset.h"

#include "amount.h"
#include "asyncrpcoperation_common.h"
#include "asyncrpcqueue.h"
#include "consensus/upgrades.h"
#include "core_io.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "script/interpreter.h"
#include "utiltime.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "sodium.h"
#include "miner.h"
#include "wallet/paymentdisclosuredb.h"

#include <array>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

using namespace libzcash;

AsyncRPCOperation_issueasset::AsyncRPCOperation_issueasset(
        TransactionBuilder builder,
        std::vector<IssueAssetUTXO> inputs,
        std::string toAddress,
        uint32_t assetType,
        CAmount assetAmount,
        CAmount fee,
        UniValue contextInfo) :
        builder_(builder), inputs_(inputs), assetType_(assetType), assetAmount_(assetAmount), fee_(fee), contextinfo_(contextInfo)
{
    if (fee < 0 || fee > MAX_MONEY) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Fee is out of range");
    }

    if (inputs.size() == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Empty inputs");
    }

    //  Check the destination address is valid for this network i.e. not testnet being used on mainnet
    auto address = DecodePaymentAddress(toAddress);
    if (boost::get<SaplingPaymentAddress>(&address) != nullptr) {
        tozaddr_ = boost::get<SaplingPaymentAddress>(address);
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to address");
    }

    // Log the context info
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_issueasset initialized (context=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_issueasset initialized\n", getId());
    }

    // Lock UTXOs
    lock_utxos();
}

AsyncRPCOperation_issueasset::~AsyncRPCOperation_issueasset() {
}

void AsyncRPCOperation_issueasset::main() {
    if (isCancelled()) {
        unlock_utxos(); // clean up
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

    bool success = false;

#ifdef ENABLE_MINING
    GenerateBitcoins(false, 0, Params());
#endif

    try {
        success = main_impl();
    } catch (const UniValue& objError) {
        int code = find_value(objError, "code").get_int();
        std::string message = find_value(objError, "message").get_str();
        set_error_code(code);
        set_error_message(message);
    } catch (const runtime_error& e) {
        set_error_code(-1);
        set_error_message("runtime error: " + string(e.what()));
    } catch (const logic_error& e) {
        set_error_code(-1);
        set_error_message("logic error: " + string(e.what()));
    } catch (const exception& e) {
        set_error_code(-1);
        set_error_message("general exception: " + string(e.what()));
    } catch (...) {
        set_error_code(-2);
        set_error_message("unknown error");
    }

#ifdef ENABLE_MINING
    GenerateBitcoins(GetBoolArg("-gen",false), GetArg("-genproclimit", 1), Params());
#endif

    stop_execution_clock();

    if (success) {
        set_state(OperationStatus::SUCCESS);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: z_issueasset finished (status=%s", getId(), getStateAsString());
    if (success) {
        s += strprintf(", txid=%s)\n", tx_.GetHash().ToString());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s",s);

    unlock_utxos(); // clean up
}

bool AsyncRPCOperation_issueasset::main_impl() {

    CAmount minersFee = fee_;

    CAmount inputTotal = 0;
    for (auto utxo : inputs_) {
        inputTotal += utxo.amount;
    }

    if (inputTotal <= minersFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient funds, have %s and miners fee is %s",
            FormatMoney(inputTotal), FormatMoney(minersFee)));
    }

    LogPrint("zrpc", "%s: issuing %s of asset type %d with fee %s\n",
            getId(), FormatMoney(assetAmount_), assetType_, FormatMoney(minersFee));

    builder_.SetFee(fee_);

    // Sending from a t-address, which we don't have an ovk for. Instead,
    // generate a common one from the HD seed. This ensures the data is
    // recoverable, while keeping it logically separate from the ZIP 32
    // Sapling key hierarchy, which the user might not be using.
    HDSeed seed = pwalletMain->GetHDSeedForRPC();
    uint256 ovk = ovkForShieldingFromTaddr(seed);

    // Add transparent inputs to cover fee
    for (auto t : inputs_) {
        builder_.AddTransparentInput(COutPoint(t.txid, t.vout), t.scriptPubKey, t.amount);
    }

    // Issue asset to the target z-addr
    builder_.SetValueBalanceAssetType(assetType_);
    builder_.AddSaplingOutput(ovk, tozaddr_, assetType_, assetAmount_);

    // Set change address for excess funds
    CReserveKey keyChange(pwalletMain);
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        EnsureWalletIsUnlocked();
        CPubKey vchPubKey;
        bool ret = keyChange.GetReservedKey(vchPubKey);
        if (!ret) {
            // should never fail, as we just unlocked
            throw JSONRPCError(
                RPC_WALLET_KEYPOOL_RAN_OUT,
                "Could not generate a taddr to use as a change address");
        }

        CTxDestination changeAddr = vchPubKey.GetID();
        builder_.SendChangeTo(changeAddr);
    }

    // Build the transaction
    tx_ = builder_.Build().GetTxOrThrow();

    UniValue sendResult = SendTransaction(tx_, keyChange, testmode);
    set_result(sendResult);

    return true;
}

/**
 * Override getStatus() to append the operation's context object to the default status object.
 */
UniValue AsyncRPCOperation_issueasset::getStatus() const {
    UniValue v = AsyncRPCOperation::getStatus();
    if (contextinfo_.isNull()) {
        return v;
    }

    UniValue obj = v.get_obj();
    obj.push_back(Pair("method", "z_issueasset"));
    obj.push_back(Pair("params", contextinfo_ ));
    return obj;
}

/**
 * Lock input utxos
 */
 void AsyncRPCOperation_issueasset::lock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : inputs_) {
        COutPoint outpt(utxo.txid, utxo.vout);
        pwalletMain->LockCoin(outpt);
    }
}

/**
 * Unlock input utxos
 */
void AsyncRPCOperation_issueasset::unlock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : inputs_) {
        COutPoint outpt(utxo.txid, utxo.vout);
        pwalletMain->UnlockCoin(outpt);
    }
}
