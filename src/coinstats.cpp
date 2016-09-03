// Copyright (c) 2014-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinstats.h"
#include "main.h"

#include <stdint.h>

#include <boost/thread/thread.hpp> // boost::thread::interrupt

using namespace std;

//! Calculate statistics about the unspent transaction output set
bool GetUTXOStats(CCoinsView *view, CCoinsViewByScriptDB *viewbyscriptdb, CCoinsStats &stats)
{
    std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = pcursor->GetBestBlock();
    {
        LOCK(cs_main);
        stats.nHeight = mapBlockIndex.find(stats.hashBlock)->second->nHeight;
    }
    ss << stats.hashBlock;
    CAmount nTotalAmount = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        uint256 key;
        CCoins coins;
        if (pcursor->GetKey(key) && pcursor->GetValue(coins)) {
            stats.nTransactions++;
            ss << key;
            for (unsigned int i=0; i<coins.vout.size(); i++) {
                const CTxOut &out = coins.vout[i];
                if (!out.IsNull()) {
                    stats.nTransactionOutputs++;
                    ss << VARINT(i+1);
                    ss << out;
                    nTotalAmount += out.nValue;
                }
            }
            stats.nSerializedSize += 32 + pcursor->GetValueSize();
            ss << VARINT(0);
        } else {
            return error("%s: unable to read value", __func__);
        }
        pcursor->Next();
    }
    stats.hashSerialized = ss.GetHash();
    stats.nTotalAmount = nTotalAmount;

    std::unique_ptr<CCoinsViewByScriptDBCursor> pcursordb(viewbyscriptdb->Cursor());
    while (pcursordb->Valid()) {
        boost::this_thread::interruption_point();
        uint160 hash;
        CCoinsByScript coinsByScript;
        if (pcursordb->GetKey(hash) && pcursordb->GetValue(coinsByScript)) {
            stats.nAddresses++;
            stats.nAddressesOutputs += coinsByScript.setCoins.size();
        } else {
            return error("%s: unable to read value", __func__);
        }
        pcursordb->Next();
    }
    return true;
}
