// Copyright (c) 2017-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_EVO_CBTX_H
#define SYSCOIN_EVO_CBTX_H

#include <primitives/transaction.h>
#include <univalue.h>
#include <kernel/cs_main.h>
class CBlock;
class CBlockIndex;
class BlockValidationState;
class TxValidationState;
namespace llmq 
{
    class CFinalCommitmentTxPayload;
    class CQuorumBlockProcessor;
}

class CCoinsViewCache;

// coinbase transaction
class CCbTx
{
public:
    static constexpr uint16_t CURRENT_VERSION = 2;

public:
    uint16_t nVersion{CURRENT_VERSION};
    int32_t nHeight{0};

public:
    SERIALIZE_METHODS(CCbTx, obj) {
        READWRITE(obj.nVersion, obj.nHeight);
    }

    std::string ToString() const;

    void ToJson(UniValue& obj) const
    {
        obj.clear();
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("height", nHeight);
    }
    bool IsNull() const {
        return nHeight == 0;
    }
};

bool CheckCbTx(const CTransaction& tx, const CBlockIndex* pindexPrev, TxValidationState& state, bool fJustCheck) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
bool CheckCbTx(const CCbTx &cbTx, const CBlockIndex* pindexPrev, TxValidationState& state, bool fJustCheck) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
#endif // SYSCOIN_EVO_CBTX_H
