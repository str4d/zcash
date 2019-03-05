// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pczt.h>

#include <main.h>
#include <transaction_builder.h>
#include <utilstrencodings.h>

#include <librustzcash.h>

using namespace pczt;

uint256 StrToUint256(const std::string& str)
{
    CDataStream ss(str.data(), str.data() + str.length(), SER_NETWORK, PROTOCOL_VERSION);
    uint256 ret;
    ss >> ret;
    return ret;
}

Pczt::Pczt(const Consensus::Params& consensusParams, int nHeight)
{
    auto mtx = CreateNewContextualCMutableTransaction(consensusParams, nHeight);
    this->SetFromTx(mtx);
}

Pczt::Pczt(const CMutableTransaction& mtx)
{
    this->SetFromTx(mtx);
}

void Pczt::SetFromTx(const CMutableTransaction& mtx)
{
    PcztGlobal global;
    global.set_version(mtx.nVersion);
    global.set_versiongroupid(mtx.nVersionGroupId);
    global.set_locktime(mtx.nLockTime);
    global.set_expiryheight(mtx.nExpiryHeight);
    global.set_valuebalance(mtx.valueBalance);

    for (auto spend : mtx.vShieldedSpend) {
        if (global.saplinganchor().empty()) {
            global.set_saplinganchor(spend.anchor.begin(), spend.anchor.size());
        }
    }

    this->inner.set_allocated_global(&global);
}

CMutableTransaction Pczt::ToMutableTx()
{
    CMutableTransaction mtx;

    mtx.fOverwintered = true;
    mtx.nVersion = this->inner.global().version();
    mtx.nVersionGroupId = this->inner.global().versiongroupid();
    mtx.nLockTime = this->inner.global().locktime();
    mtx.nExpiryHeight = this->inner.global().expiryheight();
    mtx.valueBalance = this->inner.global().valuebalance();

    auto saplingAnchor = StrToUint256(this->inner.global().saplinganchor());

    mtx.vShieldedSpend.resize(this->inner.spends_size());
    for (auto i = 0; i < this->inner.spends_size(); i++) {
        auto spend = this->inner.spends(i);
        mtx.vShieldedSpend[i].cv = StrToUint256(spend.cv());
        mtx.vShieldedSpend[i].anchor = saplingAnchor;
        mtx.vShieldedSpend[i].nullifier = StrToUint256(spend.nf());
        mtx.vShieldedSpend[i].rk = StrToUint256(spend.rk());

        auto zkproof = spend.zkproof();
        if (zkproof.size() != mtx.vShieldedSpend[i].zkproof.size()) {
            throw std::runtime_error("zkproof is wrong size");
        }
        std::copy(zkproof.begin(), zkproof.end(), mtx.vShieldedSpend[i].zkproof.begin());

        auto spendAuthSig = spend.spendauthsig();
        if (spendAuthSig.size() != mtx.vShieldedSpend[i].spendAuthSig.size()) {
            throw std::runtime_error("spendAuthSig is wrong size");
        }
        std::copy(spendAuthSig.begin(), spendAuthSig.end(), mtx.vShieldedSpend[i].spendAuthSig.begin());
    }

    mtx.vShieldedOutput.resize(this->inner.outputs_size());
    for (auto i = 0; i < this->inner.outputs_size(); i++) {
        auto output = this->inner.outputs(i);
        mtx.vShieldedOutput[i].cv = StrToUint256(output.cv());
        mtx.vShieldedOutput[i].cmu = StrToUint256(output.cmu());
        mtx.vShieldedOutput[i].ephemeralKey = StrToUint256(output.epk());

        auto encCiphertext = output.encciphertext();
        if (encCiphertext.size() != mtx.vShieldedOutput[i].encCiphertext.size()) {
            throw std::runtime_error("encCiphertext is wrong size");
        }
        std::copy(encCiphertext.begin(), encCiphertext.end(), mtx.vShieldedOutput[i].encCiphertext.begin());

        auto outCiphertext = output.outciphertext();
        if (outCiphertext.size() != mtx.vShieldedOutput[i].outCiphertext.size()) {
            throw std::runtime_error("outCiphertext is wrong size");
        }
        std::copy(outCiphertext.begin(), outCiphertext.end(), mtx.vShieldedOutput[i].outCiphertext.begin());

        auto zkproof = output.zkproof();
        if (zkproof.size() != mtx.vShieldedOutput[i].zkproof.size()) {
            throw std::runtime_error("zkproof is wrong size");
        }
        std::copy(zkproof.begin(), zkproof.end(), mtx.vShieldedOutput[i].zkproof.begin());
    }

    return mtx;
}

bool Pczt::Parse(const std::string& encoded, std::string& error)
{
    auto decoded = DecodeBase64(encoded);
    if (decoded.empty()) {
        error = "invalid base64";
        return false;
    }
    if (!this->inner.ParseFromString(decoded)) {
        error = "invalid PCZT";
        return false;
    }
    return true;
}

std::string Pczt::Serialize()
{
    return inner.SerializeAsString();
}

void Pczt::SetSaplingAnchor(uint256 anchor)
{
    // Sanity check: cannot add Sapling anchor to pre-Sapling transaction
    if (this->inner.global().version() < SAPLING_TX_VERSION) {
        throw std::runtime_error("TransactionBuilder cannot add Sapling spend to pre-Sapling transaction");
    }

    this->inner.mutable_global()->set_saplinganchor(anchor.begin(), anchor.size());
}

void Pczt::AddSaplingSpend(
    pczt::Zip32Key& zip32Key,
    libzcash::SaplingExpandedSpendingKey expsk,
    libzcash::SaplingNote note,
    SaplingWitness witness)
{
    // Sanity check: cannot add Sapling spend to pre-Sapling transaction
    auto global = this->inner.global();
    if (global.version() < SAPLING_TX_VERSION) {
        throw std::runtime_error("Cannot add Sapling spend to pre-Sapling transaction");
    }

    // Consistency check: Sapling anchor must be set
    if (global.saplinganchor().empty()) {
        throw std::runtime_error("Must call SetSaplingAnchor() first.");
    }

    // Consistency check: all witness anchors must equal the set anchor
    uint256 anchor = StrToUint256(global.saplinganchor());
    if (anchor != witness.root()) {
        throw std::runtime_error("Witness anchor does not match specified Sapling anchor.");
    }

    // Create Sapling SpendDescriptions
    auto cmu = note.cmu();
    auto nf = note.nullifier(expsk.full_viewing_key(), witness.position());
    if (!cmu || !nf) {
        throw std::runtime_error("Spend is invalid");
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << witness.path();
    std::vector<unsigned char> witness_data(ss.begin(), ss.end());

    uint256 alpha;
    librustzcash_sapling_generate_r(alpha.begin());

    // Create the Sapling proving context
    void* ctx;
    if (this->inner.global().bsk().empty() || this->inner.global().cvsum().empty()) {
        if (this->inner.spends_size() > 0 || this->inner.outputs_size() > 0) {
            throw std::runtime_error("Invalid PCZT");
        }
        ctx = librustzcash_sapling_proving_ctx_init();
    } else {
        ctx = librustzcash_sapling_proving_ctx_init_from_parts(
            StrToUint256(this->inner.global().bsk()).begin(),
            StrToUint256(this->inner.global().cvsum()).begin());
    }

    // Create the Spend proof
    uint256 rcv;
    SpendDescription sdesc;
    if (!librustzcash_sapling_spend_proof(
            ctx,
            expsk.full_viewing_key().ak.begin(),
            expsk.nsk.begin(),
            note.d.data(),
            note.r.begin(),
            alpha.begin(),
            note.value(),
            anchor.begin(),
            witness_data.data(),
            rcv.begin(),
            sdesc.cv.begin(),
            sdesc.rk.begin(),
            sdesc.zkproof.data())) {
        librustzcash_sapling_proving_ctx_free(ctx);
        throw std::runtime_error("Spend proof failed");
    }

    // Grab the updated bsk and cv_sum
    uint256 bsk;
    uint256 cv_sum;
    librustzcash_sapling_proving_ctx_into_parts(ctx, bsk.begin(), cv_sum.begin());

    // Update PCZT with new spend
    auto spend = this->inner.add_spends();
    spend->set_cv(sdesc.cv.begin(), sdesc.cv.size());
    spend->set_nf(nf->begin(), nf->size());
    spend->set_rk(sdesc.rk.begin(), sdesc.rk.size());
    spend->set_zkproof(sdesc.zkproof.data(), sdesc.zkproof.size());
    spend->set_alpha(alpha.begin(), alpha.size());
    spend->set_value(note.value());
    spend->set_rcv(rcv.begin(), rcv.size());
    spend->mutable_key()->CopyFrom(zip32Key);
    this->inner.mutable_global()->set_valuebalance(global.valuebalance() + note.value());
    this->inner.mutable_global()->set_bsk(bsk.begin(), bsk.size());
    this->inner.mutable_global()->set_cvsum(cv_sum.begin(), cv_sum.size());
}

void Pczt::AddSaplingOutput(
    pczt::Zip32Key& zip32Key,
    uint256 ovk,
    libzcash::SaplingPaymentAddress to,
    CAmount value,
    std::array<unsigned char, ZC_MEMO_SIZE> memo)
{
    // Sanity check: cannot add Sapling spend to pre-Sapling transaction
    auto global = this->inner.global();
    if (global.version() < SAPLING_TX_VERSION) {
        throw std::runtime_error("Cannot add Sapling spend to pre-Sapling transaction");
    }

    auto note = libzcash::SaplingNote(to, value);

    // Create the Sapling proving context
    void* ctx;
    if (global.bsk().empty() || global.cvsum().empty()) {
        if (this->inner.spends_size() > 0 || this->inner.outputs_size() > 0) {
            throw std::runtime_error("Invalid PCZT");
        }
        ctx = librustzcash_sapling_proving_ctx_init();
    } else {
        ctx = librustzcash_sapling_proving_ctx_init_from_parts(
            StrToUint256(global.bsk()).begin(),
            StrToUint256(global.cvsum()).begin());
    }
    if (ctx == nullptr) {
        throw std::runtime_error("Invalid bsk or cv_sum");
    }

    // Create the Output description
    auto res = OutputDescriptionInfo(ovk, note, memo).Build(ctx);
    if (!res) {
        librustzcash_sapling_proving_ctx_free(ctx);
        throw std::runtime_error("Failed to add Sapling output");
    }
    auto odesc = res.get().first;
    auto rcv = res.get().second;

    // Grab the updated bsk and cv_sum
    uint256 bsk;
    uint256 cv_sum;
    librustzcash_sapling_proving_ctx_into_parts(ctx, bsk.begin(), cv_sum.begin());

    // Update PCZT with new output
    auto output = this->inner.add_outputs();
    output->set_cv(odesc.cv.begin(), odesc.cv.size());
    output->set_cmu(odesc.cmu.begin(), odesc.cmu.size());
    output->set_epk(odesc.ephemeralKey.begin(), odesc.ephemeralKey.size());
    output->set_encciphertext(odesc.encCiphertext.data(), odesc.encCiphertext.size());
    output->set_outciphertext(odesc.outCiphertext.data(), odesc.outCiphertext.size());
    output->set_zkproof(odesc.zkproof.data(), odesc.zkproof.size());
    output->set_value(value);
    output->set_rcv(rcv.begin(), rcv.size());
    output->mutable_key()->CopyFrom(zip32Key);
    this->inner.mutable_global()->set_valuebalance(global.valuebalance() - value);
    this->inner.mutable_global()->set_bsk(bsk.begin(), bsk.size());
    this->inner.mutable_global()->set_cvsum(cv_sum.begin(), cv_sum.size());
}

CAmount Pczt::GetFee()
{
    CAmount fee = this->inner.global().valuebalance();
    // No transparent inputs or outputs
    return fee;
}

CTransaction Pczt::Finalize(int nHeight, const Consensus::Params& params)
{
    // Check we have sensible funds
    if (this->GetFee() < 0) {
        throw std::runtime_error("Negative fee");
    }

    // Check that we have spendAuthSigs for every spend
    for (auto i = 0; i < this->inner.spends_size(); i++) {
        if (this->inner.spends(i).spendauthsig().empty()) {
            throw std::runtime_error("Missing spendAuthSig in spend " + i);
        }
    }

    auto mtx = this->ToMutableTx();

    auto consensusBranchId = CurrentEpochBranchId(nHeight, params);

    // Empty output script.
    uint256 dataToBeSigned;
    CScript scriptCode;
    try {
        dataToBeSigned = SignatureHash(scriptCode, mtx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);
    } catch (std::logic_error ex) {
        throw std::runtime_error("Could not construct signature hash: " + std::string(ex.what()));
    }

    if (this->inner.global().bsk().empty() || this->inner.global().cvsum().empty()) {
        throw std::runtime_error("Invalid PCZT");
    }
    auto ctx = librustzcash_sapling_proving_ctx_init_from_parts(
        StrToUint256(this->inner.global().bsk()).begin(),
        StrToUint256(this->inner.global().cvsum()).begin());
    if (ctx == nullptr) {
        throw std::runtime_error("Invalid bsk or cv_sum");
    }

    librustzcash_sapling_binding_sig(
        ctx,
        mtx.valueBalance,
        dataToBeSigned.begin(),
        mtx.bindingSig.data());

    librustzcash_sapling_proving_ctx_free(ctx);

    return CTransaction(mtx);
}
