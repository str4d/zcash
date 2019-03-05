// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCASH_PCZT_H
#define ZCASH_PCZT_H

#include <consensus/params.h>
#include <pczt.pb.h>
#include <primitives/transaction.h>

uint256 StrToUint256(const std::string& str);

class Pczt
{
private:
    pczt::PartiallyCreatedTransaction inner;

    void SetFromTx(const CMutableTransaction& mtx);

    CMutableTransaction ToMutableTx();

public:
    Pczt() {}
    Pczt(const Consensus::Params& consensusParams, int nHeight);
    Pczt(const CMutableTransaction& mtx);

    bool Parse(const std::string& encoded, std::string& error);
    std::string Serialize();

    const pczt::PartiallyCreatedTransaction& Data()
    {
        return inner;
    }

    void SetSaplingAnchor(uint256 anchor);

    void AddSaplingSpend(
        pczt::Zip32Key& zip32Key,
        libzcash::SaplingExpandedSpendingKey expsk,
        libzcash::SaplingNote note,
        SaplingWitness witness);

    void AddSaplingOutput(
        pczt::Zip32Key& zip32Key,
        uint256 ovk,
        libzcash::SaplingPaymentAddress to,
        CAmount value,
        std::array<unsigned char, ZC_MEMO_SIZE> memo = {{0xF6}});

    CAmount GetFee();

    CTransaction Finalize(int nHeight, const Consensus::Params& params);
};

#endif /* ZCASH_PCZT_H */
