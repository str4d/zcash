// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "i2p.h"

#include "util.h"

#include <libi2pd/Config.h>
#include <libi2pd/Crypto.h>
#include <libi2pd/FS.h>
#include <libi2pd/RouterContext.h>
#include <libi2pd/api.h>

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
}

void InterruptI2P()
{
    // i2pd does not have separate APIs for interrupting and stopping
}

void StopI2P()
{
    LogPrintf("%s: stopping router\n", __func__);
    i2p::api::StopI2P();
    i2p::api::TerminateI2P();
}
