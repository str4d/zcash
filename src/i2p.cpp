// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "i2p.h"

#include "net.h"
#include "serialize.h"
#include "util.h"
#include "version.h"

#include <libi2pd/Config.h>
#include <libi2pd/Crypto.h>
#include <libi2pd/FS.h>
#include <libi2pd/RouterContext.h>
#include <libi2pd/api.h>

const size_t TX_BUFFER_SIZE = 2 * 1000 * 1000;

// Destination for relaying our transactions outwards.
std::shared_ptr<i2p::client::ClientDestination> relayOutbound;

// Destination for receiving others' transactions to inject into the network.
std::shared_ptr<i2p::client::ClientDestination> relayInbound;

void RelayTransactionOverI2P(const CTransaction& tx)
{
    // Pick a peer to relay the transaction to.
    // TODO: For now, we relay to ourselves
    auto recipient = relayInbound->GetIdentHash();

    LogPrintf("%s: Opening stream to %s\n", __func__, recipient.ToBase64());
    std::shared_ptr<i2p::stream::Stream> s = nullptr;
    for (int i = 0; i < 60; i++) {
        s = i2p::api::CreateStream(relayOutbound, recipient);
        if (s) {
            break;
        }
        MilliSleep(1000);
        boost::this_thread::interruption_point();
    }

    if (s) {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss.reserve(10000);
        ss << tx;
        std::vector<uint8_t> buf(ss.begin(), ss.end());

        LogPrintf("%s: Sending %d bytes to %s\n", __func__, buf.size(), recipient.ToBase64());
        s->AsyncSend(buf.data(), buf.size(), [s](const boost::system::error_code& ecode) {
            if (ecode) {
                LogPrintf("RelayTransactionOverI2P: Error while sending to peer: %s\n", ecode.message());
            }
            i2p::api::DestroyStream(s);
        });
    } else {
        LogPrintf("%s: Failed to create stream!\n", __func__);
    }
}

void ForwardTransactionFromI2P(
    char buffer[TX_BUFFER_SIZE],
    const boost::system::error_code &ecode,
    size_t received)
{
    if (ecode) {
        LogPrintf("%s: Error while reading from peer: %s\n", __func__, ecode.message());
    } else {
        LogPrintf("%s: Received %d bytes from peer\n", __func__, received);
        auto start = static_cast<const char*>(&buffer[0]);
        CDataStream ss(start, start + received, SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        try {
            ss >> tx;
            RelayTransaction(tx);
        } catch (std::exception &e) {
            LogPrintf("%s: Error while parsing transaction: %s\n", __func__, e.what());
        }
    }
}

void I2PAcceptor(std::shared_ptr<i2p::stream::Stream> sIn) {
    if (sIn != nullptr) {
        LogPrintf(
            "%s: Incoming transaction from %s\n",
            __func__,
            sIn->GetRemoteIdentity()->GetIdentHash().ToBase64());

        char buffer[TX_BUFFER_SIZE];
        sIn->AsyncReceive(
            boost::asio::buffer(buffer, sizeof(buffer)),
            std::bind(&ForwardTransactionFromI2P, buffer, std::placeholders::_1, std::placeholders::_2),
            60);
    } else {
        LogPrintf("%s: Listener closing\n", __func__);
    }
}

void StartI2P(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    auto datadir = GetDataDir() / "i2pd";

    // Fake argv so we can call ParseCmdline
    char* argv[] = {
        (char*)"zcashd",
        nullptr
    };

    i2p::config::Init();
    i2p::config::ParseCmdline(1, argv, true); // ignore unknown options and help
    i2p::config::Finalize();

    LogPrintf("%s: starting FS\n", __func__);
    i2p::fs::SetAppName("zcashd");
    i2p::fs::DetectDataDir(datadir.string(), false);
    i2p::fs::Init();

    LogPrintf("%s: initializing Crypto\n", __func__);
    bool precomputation;
    i2p::config::GetOption("precomputation.elgamal", precomputation);
    i2p::crypto::InitCrypto(precomputation);

    int netID;
    i2p::config::GetOption("netid", netID);
    i2p::context.SetNetID(netID);

    LogPrintf("%s: initializing RouterContext\n", __func__);
    i2p::context.Init();

    LogPrintf("%s: starting router\n", __func__);
    i2p::api::StartI2P();

    // Create Destinations for sending and receiving transactions.
    auto sigType = i2p::data::SIGNING_KEY_TYPE_EDDSA_SHA512_ED25519;
    relayOutbound = i2p::api::CreateLocalDestination(false, sigType);
    relayInbound = i2p::api::CreateLocalDestination(true, sigType);

    // Start accepting incoming transactions from peers.
    i2p::api::AcceptStream(relayInbound, I2PAcceptor);
}

void InterruptI2P()
{
    // i2pd does not have separate APIs for interrupting and stopping
}

void StopI2P()
{
    i2p::api::DestroyLocalDestination(relayOutbound);
    i2p::api::DestroyLocalDestination(relayInbound);

    LogPrintf("%s: stopping router\n", __func__);
    i2p::api::StopI2P();
    i2p::api::TerminateI2P();
}
