#include <script/interpreter.h>
#include <main.h>
#include <pubkey.h>
#include "tx_creation_utils.h"

CMutableTransaction txCreationUtils::populateTx(int txVersion, const uint256 & newScId, const CAmount & creationTxAmount, const CAmount & fwdTxAmount, int epochLength)
{
    CMutableTransaction mtx;
    mtx.nVersion = txVersion;

    mtx.vin.resize(2);
    mtx.vin[0].prevout.hash = uint256S("1");
    mtx.vin[0].prevout.n = 0;
    mtx.vin[1].prevout.hash = uint256S("2");
    mtx.vin[1].prevout.n = 0;

    mtx.vout.resize(2);
    mtx.vout[0].nValue = 0;
    mtx.vout[1].nValue = 0;

    mtx.vjoinsplit.push_back(
            JSDescription::getNewInstance(txVersion == GROTH_TX_VERSION));
    mtx.vjoinsplit.push_back(
            JSDescription::getNewInstance(txVersion == GROTH_TX_VERSION));
    mtx.vjoinsplit[0].nullifiers.at(0) = uint256S("0");
    mtx.vjoinsplit[0].nullifiers.at(1) = uint256S("1");
    mtx.vjoinsplit[1].nullifiers.at(0) = uint256S("2");
    mtx.vjoinsplit[1].nullifiers.at(1) = uint256S("3");

    mtx.vsc_ccout.resize(1);
    mtx.vsc_ccout[0].scId = newScId;
    mtx.vsc_ccout[0].nValue = creationTxAmount;
    mtx.vsc_ccout[0].withdrawalEpochLength = epochLength;

    mtx.vft_ccout.resize(1);
    mtx.vft_ccout[0].scId = mtx.vsc_ccout[0].scId;
    mtx.vft_ccout[0].nValue = fwdTxAmount;

    return mtx;
}

void txCreationUtils::signTx(CMutableTransaction& mtx)
{
    // Generate an ephemeral keypair.
    uint256 joinSplitPubKey;
    unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(joinSplitPubKey.begin(), joinSplitPrivKey);
    mtx.joinSplitPubKey = joinSplitPubKey;
    // Compute the correct hSig.
    // TODO: #966.
    static const uint256 one(uint256S("1"));
    // Empty output script.
    CScript scriptCode;
    CTransaction signTx(mtx);
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL);
    if (dataToBeSigned == one) {
        throw std::runtime_error("SignatureHash failed");
    }
    // Add the signature
    assert(crypto_sign_detached(&mtx.joinSplitSig[0], NULL, dataToBeSigned.begin(), 32, joinSplitPrivKey ) == 0);
}

CTransaction txCreationUtils::createNewSidechainTxWith(const uint256 & newScId, const CAmount & creationTxAmount, int epochLength)
{
    CMutableTransaction mtx = populateTx(SC_TX_VERSION, newScId, creationTxAmount, CAmount(0), epochLength);
    mtx.vout.resize(0);
    mtx.vjoinsplit.resize(0);
    mtx.vft_ccout.resize(0);
    signTx(mtx);

    return CTransaction(mtx);
}

CTransaction txCreationUtils::createFwdTransferTxWith(const uint256 & newScId, const CAmount & fwdTxAmount)
{
    CMutableTransaction mtx = populateTx(SC_TX_VERSION, newScId, CAmount(0), fwdTxAmount);
    mtx.vout.resize(0);
    mtx.vjoinsplit.resize(0);
    mtx.vsc_ccout.resize(0);
    signTx(mtx);

    return CTransaction(mtx);
}

// Well-formatted transparent txs have no sc-related info.
// ccisNull allow you to create a faulty transparent tx, for testing purposes.
CTransaction txCreationUtils::createTransparentTx(bool ccIsNull)
{
    CMutableTransaction mtx = populateTx(TRANSPARENT_TX_VERSION);
    mtx.vjoinsplit.resize(0);

    if (ccIsNull)
    {
        mtx.vsc_ccout.resize(0);
        mtx.vft_ccout.resize(0);
    }
    signTx(mtx);

    return CTransaction(mtx);
}

CTransaction txCreationUtils::createSproutTx(bool ccIsNull)
{
    CMutableTransaction mtx;

    if (ccIsNull)
    {
        mtx = populateTx(PHGR_TX_VERSION);
        mtx.vsc_ccout.resize(0);
        mtx.vft_ccout.resize(0);
    } else
    {
        mtx = populateTx(SC_TX_VERSION);
    }
    signTx(mtx);

    return CTransaction(mtx);
}

void txCreationUtils::extendTransaction(CTransaction & tx, const uint256 & scId, const CAmount & amount)
{
    CMutableTransaction mtx = tx;

    mtx.nVersion = SC_TX_VERSION;

    CTxScCreationOut aSidechainCreationTx;
    aSidechainCreationTx.scId = scId;
    mtx.vsc_ccout.push_back(aSidechainCreationTx);

    CTxForwardTransferOut aForwardTransferTx;
    aForwardTransferTx.scId = aSidechainCreationTx.scId;
    aForwardTransferTx.nValue = amount;
    mtx.vft_ccout.push_back(aForwardTransferTx);

    tx = mtx;
    return;
}

CScCertificate txCreationUtils::createCertificate(const uint256 & scId, int epochNum, const uint256 & endEpochBlockHash, unsigned int numChangeOut, CAmount bwTotaltAmount, unsigned int numBwt) {
    CMutableScCertificate res;
    res.nVersion = SC_CERT_VERSION;
    res.scId = scId;
    res.epochNumber = epochNum;
    res.endEpochBlockHash = endEpochBlockHash;

    res.vout.resize(numChangeOut+numBwt);
    for(unsigned int idx = 0; idx < numChangeOut; ++idx) {
    res.vout[idx].nValue = insecure_rand();
    res.vout[idx].scriptPubKey = GetScriptForDestination(CKeyID(uint160(ParseHex("816115944e077fe7c803cfa57f29b36bf87c1d35"))),/*withCheckBlockAtHeight*/false);
    res.vout[idx].isFromBackwardTransfer = false;
    }

    for(unsigned int idx = 0; idx < numBwt; ++idx) {
        res.vout[numChangeOut+idx].nValue = bwTotaltAmount/numBwt;
        res.vout[numChangeOut+idx].scriptPubKey = GetScriptForDestination(CKeyID(uint160(ParseHex("816115944e077fe7c803cfa57f29b36bf87c1d35"))),/*withCheckBlockAtHeight*/false);
        res.vout[numChangeOut+idx].isFromBackwardTransfer = true;
    }

    return res;
}

void chainSettingUtils::GenerateChainActive(int targetHeight) {
    chainActive.SetTip(nullptr);
    mapBlockIndex.clear();

    std::vector<uint256> blockHashes(targetHeight);
    std::vector<CBlockIndex> blocks(targetHeight);

    for (unsigned int height=0; height<blocks.size(); ++height) {
        blockHashes[height] = ArithToUint256(height);

        blocks[height].nHeight = height+1;
        blocks[height].pprev = height == 0? nullptr : &blocks[height - 1];
        blocks[height].phashBlock = &blockHashes[height];
        blocks[height].nTime = 1269211443 + height * Params().GetConsensus().nPowTargetSpacing;
        blocks[height].nBits = 0x1e7fffff;
        blocks[height].nChainWork = height == 0 ? arith_uint256(0) : blocks[height - 1].nChainWork + GetBlockProof(blocks[height - 1]);

        mapBlockIndex[blockHashes[height]] = &blocks[height];
    }

    chainActive.SetTip(&blocks.back());
}