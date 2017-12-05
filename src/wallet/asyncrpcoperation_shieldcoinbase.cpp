// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asyncrpcqueue.h"
#include "amount.h"
#include "consensus/upgrades.h"
#include "core_io.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"
#include "script/interpreter.h"
#include "utiltime.h"
#include "rpcprotocol.h"
#include "wallet/transaction_builder.h"
#include "zcash/IncrementalMerkleTree.hpp"
#include "sodium.h"
#include "miner.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>

#include "asyncrpcoperation_shieldcoinbase.h"

#include "paymentdisclosure.h"
#include "paymentdisclosuredb.h"

using namespace libzcash;

AsyncRPCOperation_shieldcoinbase::AsyncRPCOperation_shieldcoinbase(
        CMutableTransaction contextualTx,
        std::vector<ShieldCoinbaseUTXO> inputs,
        std::string toAddress,
        CAmount fee,
        UniValue contextInfo) :
        tx_(contextualTx), inputs_(inputs), fee_(fee), contextinfo_(contextInfo)
{
    assert(contextualTx.nVersion >= 2);  // transaction format version must support vjoinsplit

    if (fee < 0 || fee > MAX_MONEY) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Fee is out of range");
    }

    if (inputs.size() == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Empty inputs");
    }

    //  Check the destination address is valid for this network i.e. not testnet being used on mainnet
    CZCPaymentAddress address(toAddress);
    try {
        tozaddr_ = address.Get();
    } catch (const std::runtime_error& e) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("runtime error: ") + e.what());
    }

    // Log the context info
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_shieldcoinbase initialized (context=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_shieldcoinbase initialized\n", getId());
    }

    // Lock UTXOs
    lock_utxos();

    // Enable payment disclosure if requested
    paymentDisclosureMode = fExperimentalMode && GetBoolArg("-paymentdisclosure", false);
}

AsyncRPCOperation_shieldcoinbase::~AsyncRPCOperation_shieldcoinbase() {
}

void AsyncRPCOperation_shieldcoinbase::main() {
    if (isCancelled()) {
        unlock_utxos(); // clean up
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

    bool success = false;

#ifdef ENABLE_MINING
  #ifdef ENABLE_WALLET
    GenerateBitcoins(false, NULL, 0);
  #else
    GenerateBitcoins(false, 0);
  #endif
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
  #ifdef ENABLE_WALLET
    GenerateBitcoins(GetBoolArg("-gen",false), pwalletMain, GetArg("-genproclimit", 1));
  #else
    GenerateBitcoins(GetBoolArg("-gen",false), GetArg("-genproclimit", 1));
  #endif
#endif

    stop_execution_clock();

    if (success) {
        set_state(OperationStatus::SUCCESS);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: z_shieldcoinbase finished (status=%s", getId(), getStateAsString());
    if (success) {
        s += strprintf(", txid=%s)\n", tx_.GetHash().ToString());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s",s);

    unlock_utxos(); // clean up

    SavePaymentDisclosureData();
}


bool AsyncRPCOperation_shieldcoinbase::main_impl() {

    CAmount minersFee = fee_;

    size_t numInputs = inputs_.size();

    // Check mempooltxinputlimit to avoid creating a transaction which the local mempool rejects
    size_t limit = (size_t)GetArg("-mempooltxinputlimit", 0);
    {
        LOCK(cs_main);
        if (NetworkUpgradeActive(chainActive.Height() + 1, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER)) {
            limit = 0;
        }
    }
    if (limit>0 && numInputs > limit) {
        throw JSONRPCError(RPC_WALLET_ERROR,
            strprintf("Number of inputs %d is greater than mempooltxinputlimit of %d",
            numInputs, limit));
    }

    CAmount targetAmount = 0;
    for (ShieldCoinbaseUTXO & utxo : inputs_) {
        targetAmount += utxo.amount;
    }

    if (targetAmount <= minersFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
            strprintf("Insufficient coinbase funds, have %s and miners fee is %s",
            FormatMoney(targetAmount), FormatMoney(minersFee)));
    }

    CAmount sendAmount = targetAmount - minersFee;
    LogPrint("zrpc", "%s: spending %s to shield %s with fee %s\n",
            getId(), FormatMoney(targetAmount), FormatMoney(sendAmount), FormatMoney(minersFee));

    // update the transaction with these inputs
    for (ShieldCoinbaseUTXO & t : inputs_) {
        AddTransparentInput(COutPoint(t.txid, t.vout), t.amount, true);
    }

    PrepareForShielded();

    // Create joinsplit
    UniValue obj(UniValue::VOBJ);
    JoinSplitInfo info;
    info.vpub_old = sendAmount;
    info.vpub_new = 0;
    JSOutput jso = JSOutput(tozaddr_, sendAmount);
    info.vjsout.push_back(jso);
    obj = perform_joinsplit(info);
    AddShieldedOutput();
    GetProofs();

    SignTransparent();
    set_result(Send());
    return true;
}


/**
 * Override getStatus() to append the operation's context object to the default status object.
 */
UniValue AsyncRPCOperation_shieldcoinbase::getStatus() const {
    UniValue v = AsyncRPCOperation::getStatus();
    if (contextinfo_.isNull()) {
        return v;
    }

    UniValue obj = v.get_obj();
    obj.push_back(Pair("method", "z_shieldcoinbase"));
    obj.push_back(Pair("params", contextinfo_ ));
    return obj;
}

/**
 * Lock input utxos
 */
 void AsyncRPCOperation_shieldcoinbase::lock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : inputs_) {
        COutPoint outpt(utxo.txid, utxo.vout);
        pwalletMain->LockCoin(outpt);
    }
}

/**
 * Unlock input utxos
 */
void AsyncRPCOperation_shieldcoinbase::unlock_utxos() {
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : inputs_) {
        COutPoint outpt(utxo.txid, utxo.vout);
        pwalletMain->UnlockCoin(outpt);
    }
}
