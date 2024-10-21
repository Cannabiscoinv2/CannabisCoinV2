// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "primitives/block.h"
#include "uint256.h"
#include "bignum.h"

#include <cmath>

unsigned int GetNextWorkRequired2(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params& params)
{
    arith_uint256 bnProofOfWorkLimit;
	bnProofOfWorkLimit.SetCompact(UintToArith256(params.powLimit).GetCompact());
    /* current difficulty formula, - DarkGravity v3, written by Evan Duffield - evan@dashpay.io */
    const CBlockIndex* BlockLastSolved = pindexLast;
    const CBlockIndex* BlockReading = pindexLast;
    int64_t nActualTimespan = 0;
    int64_t LastBlockTime = 0;
    int64_t PastBlocksMin = 24;
    int64_t PastBlocksMax = 24;
    int64_t CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin) {
        return bnProofOfWorkLimit.GetCompact();
    }

    arith_uint256 bnTargetLimit = bnProofOfWorkLimit;
    int64_t nTargetSpacing = 90;
    int64_t nTargetTimespan = 60 * 40;
    int64_t nActualSpacing = 0;
    if (pindexLast->nHeight != 0)
        nActualSpacing = pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime();
    if (nActualSpacing < 0)
        nActualSpacing = 1;
    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    int64_t nInterval = nTargetTimespan / nTargetSpacing;

    int64_t numerator = (nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing;
    int64_t denominator = (nInterval + 1) * nTargetSpacing;

    bnNew *= numerator;
    bnNew /= denominator;

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    if (pindexLast->nHeight + 1 == params.nForkThree)
    //returns min difficulty for nForkThree block
    return 0x1d00ffff;
        
    if (pindexLast->nHeight + 1 >= params.nForkFour)
        GetNextWorkRequired2(pindexLast, pblock, params);
    
    CBigNum bnProofOfWorkLimit;
	bnProofOfWorkLimit.SetCompact(UintToArith256(params.powLimit).GetCompact());

    // RegTest - Do not change difficulty
    if (params.fPowAllowMinDifficultyBlocks)
        return pindexLast->nBits;

    const int64 TargetBlocksSpacingSeconds = params.nPowTargetSpacing;
    const unsigned int TimeDaySeconds = params.nPowTargetTimespan;
    int64 PastSecondsMin = TimeDaySeconds * 0.025;
    int64 PastSecondsMax = TimeDaySeconds * 7;
    uint64 PastBlocksMin = PastSecondsMin / TargetBlocksSpacingSeconds;
    uint64 PastBlocksMax = PastSecondsMax / TargetBlocksSpacingSeconds;
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    uint64 PastBlocksMass = 0;
    int64 PastRateActualSeconds = 0;
    int64 PastRateTargetSeconds = 0;
    double PastRateAdjustmentRatio = double(1);
    CBigNum PastDifficultyAverage;
    CBigNum PastDifficultyAveragePrev;
    double EventHorizonDeviation;
    double EventHorizonDeviationFast;
    double EventHorizonDeviationSlow;
        
    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64)BlockLastSolved->nHeight < PastBlocksMin)
        return bnProofOfWorkLimit.GetCompact();

    int64 LatestBlockTime = BlockLastSolved->GetBlockTime();
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax)
            break;

        PastBlocksMass++;

        if (i == 1)
            PastDifficultyAverage.SetCompact(BlockReading->nBits);
        else
            PastDifficultyAverage = ((CBigNum().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev;

        PastDifficultyAveragePrev = PastDifficultyAverage;
                
        if (LatestBlockTime < BlockReading->GetBlockTime())
            if ((BlockReading->nHeight > 1) || (Params().NetworkIDString() == CBaseChainParams::TESTNET && (BlockReading->nHeight >= 10)))
                LatestBlockTime = BlockReading->GetBlockTime();

        PastRateActualSeconds = LatestBlockTime - BlockReading->GetBlockTime();
        PastRateTargetSeconds = TargetBlocksSpacingSeconds * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);
        if ((BlockReading->nHeight > 1) || (Params().NetworkIDString() == CBaseChainParams::TESTNET && (BlockReading->nHeight >= 10))) {
            if (PastRateActualSeconds < 1)
                    PastRateActualSeconds = 1;
        } else {
            if (PastRateActualSeconds < 0)
                PastRateActualSeconds = 0;
        }

        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);

        EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass)/double(28.2)), -1.228));
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1 / EventHorizonDeviation;
                
        if (PastBlocksMass >= PastBlocksMin) {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)) {
                assert(BlockReading);
                break;
            }
        }
        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    CBigNum bnNew(PastDifficultyAverage);
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        bnNew *= PastRateActualSeconds;
        bnNew /= PastRateTargetSeconds;
    }

    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
