#include "arith_uint256.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "util.h"
#include "primitives/block.h"
#include "serialize.h"
#include "streams.h"
#include "utiltime.h"
#include "version.h"

#include <iostream>

void mine(int n, int k, uint32_t d)
{
    CBlock pblock;
    pblock.nBits = d;
    arith_uint256 hashTarget = arith_uint256().SetCompact(d);

    while (true) {
        // Hash state
        crypto_generichash_blake2b_state state;
        EhInitialiseState(n, k, state);

        // I = the block header minus nonce and solution.
        CEquihashInput I{ pblock };
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;

        // H(I||...
        crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

        // Find valid nonce
        int64_t nStart = GetTime();
        while (true) {
            // H(I||V||...
            crypto_generichash_blake2b_state curr_state;
            curr_state = state;
            crypto_generichash_blake2b_update(&curr_state, pblock.nNonce.begin(), pblock.nNonce.size());

            // (x_1, x_2, ...) = A(I, V, n, k)
            std::cout << "Running Equihash solver with nNonce = " << pblock.nNonce.ToString() << "\n";
            std::function<bool(std::vector<unsigned char>)> validBlock = [&pblock, &hashTarget](
                std::vector<unsigned char> soln) {
                // Write the solution to the hash and compute the result.
                LogPrint("pow", "- Checking solution against target\n");
                pblock.nSolution = soln;

                if (UintToArith256(pblock.GetHash()) > hashTarget) {
                    return false;
                }

                // Found a solution
                LogPrintf("ZcashMiner:\n");
                LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n",
                          pblock.GetHash().GetHex(),
                          hashTarget.GetHex());

                return true;
            };

            // If we find a valid block, we rebuild
            if (EhOptimisedSolveUncancellable(n, k, curr_state, validBlock))
                break;

            if ((UintToArith256(pblock.nNonce) & 0xffff) == 0xffff)
                break;

            // Update nNonce and nTime
            pblock.nNonce = ArithToUint256(UintToArith256(pblock.nNonce) + 1);
        }

    updateblock:
        pblock.hashPrevBlock = pblock.GetHash();
    }
}

int main(int argc, char* argv[])
{
    // Zcash debugging
    fDebug = true;
    fPrintToConsole = true;
    std::vector<std::string> myDebugCategories{ "pow" };
    mapMultiArgs["-debug"] = myDebugCategories;

    // Start the mining operation
    mine(Params(CBaseChainParams::MAIN).EquihashN(), Params(CBaseChainParams::MAIN).EquihashK(), 0x207fffff);
    return 0;
}