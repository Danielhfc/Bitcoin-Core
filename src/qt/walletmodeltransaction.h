// Copyright (c) 2011-2018 The Talkcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TALKCOIN_QT_WALLETMODELTRANSACTION_H
#define TALKCOIN_QT_WALLETMODELTRANSACTION_H

#include <qt/walletmodel.h>

#include <memory>
#include <amount.h>

#include <QObject>

class SendCoinsRecipient;

namespace interfaces {
class Node;
}

/** Data model for a walletmodel transaction. */
class WalletModelTransaction
{
public:
    explicit WalletModelTransaction(const QList<SendCoinsRecipient> &recipients);

    QList<SendCoinsRecipient> getRecipients() const;

    CTransactionRef& getWtx();
    unsigned int getTransactionSize();

    void setTransactionFee(const CAmount& newFee);
    CAmount getTransactionFee() const;

    CAmount getTotalTransactionAmount() const;

    void reassignAmounts(int nChangePosRet); // needed for the subtract-fee-from-amount feature

private:
    QList<SendCoinsRecipient> recipients;
    CTransactionRef wtx;
    CAmount fee;
};

#endif // TALKCOIN_QT_WALLETMODELTRANSACTION_H
