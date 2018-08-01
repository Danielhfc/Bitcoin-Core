// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_UTIL_H
#define BITCOIN_WALLET_UTIL_H

#include <chainparamsbase.h>
#include <module-interface.h>
#include <util.h>
#include <wallet/wallet.h>

//! Get the path of the wallet directory.
fs::path GetWalletDir();

/**
 * Figures out what wallet, if any, to use for a Private Send.
 *
 * @param[in]
 * @return nullptr if no wallet should be used, or a pointer to the CWallet
 */
CWallet *GetWalletForPSRequest();

class CKeyHolder
{
private:
    CReserveKey reserveKey;
    CPubKey pubKey;
public:
    CKeyHolder(CWallet* pwalletIn);
    CKeyHolder(CKeyHolder&&) = default;
    CKeyHolder& operator=(CKeyHolder&&) = default;
    void KeepKey();
    void ReturnKey();

    CScript GetScriptForDestination() const;

};

class CKeyHolderStorage
{
private:
    std::vector<std::unique_ptr<CKeyHolder> > storage;
    mutable CCriticalSection cs_storage;

public:
    CScript AddKey(CWallet* pwalletIn);
    void KeepAll();
    void ReturnAll();

};

#endif // BITCOIN_WALLET_UTIL_H
