// Copyright (c) 2024-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_EPHEMERAL_POLICY_H
#define BITCOIN_POLICY_EPHEMERAL_POLICY_H

#include <policy/packages.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <txmempool.h>

/** These utility functions ensure that ephemeral dust is safely
 * created and spent without unduly risking them entering the utxo
 * set.

 * This is ensured by requiring:
 * - CheckValidEphemeralTx checks are respected
 * - The parent has no child (and 0-fee as implied above to disincentivize mining)
 * - OR the parent transaction has exactly one child, and the dust is spent by that child
 *
 * Imagine three transactions:
 * TxA, 0-fee with two outputs, one non-dust, one dust
 * TxB, spends TxA's non-dust
 * TxC, spends TxA's dust
 *
 * All the dust is spent if TxA+TxB+TxC is accepted, but the mining template may just pick
 * up TxA+TxB rather than the three "legal configurations:
 * 1) None
 * 2) TxA+TxB+TxC
 * 3) TxA+TxC
 * By requiring the child transaction to sweep any dust from the parent txn, we ensure that
 * there is a single child only, and this child, or the child's descendants,
 * are the only way to bring fees.
 */

/* All the following checks are only called if standardness rules are being applied. */

/** Must be called for each transaction once transaction fees are known.
 * Does context-less checks about a single transaction.
 * Returns false if the fee is non-zero and dust exists, populating state. True otherwise.
 */
bool CheckValidEphemeralTx(const CTransactionRef tx, CFeeRate dust_relay_fee, CAmount txfee, TxValidationState& state);

/** Must be called for each transaction package if any dust is in the package.
 *  Checks that all dust in a package ends up spent by an only-child or has no child.
 *  Assumes package is well-formed and sorted.
 *  The function returns std::nullopt if all dust is properly spent, or the txid of the violating child spend.
 */
std::optional<Txid> CheckEphemeralSpends(const Package& package, CFeeRate dust_relay_rate);

/** Must be called for each transaction, regardless of whether the transaction has dust.
 * Checks that individual transactions' parents have all their dust spent by this only-child transaction.
 *  The function returns std::nullopt if all dust is properly spent or an error message string.
 */
std::optional<std::string> CheckEphemeralSpends(const CTransactionRef& ptx,
                                                const CTxMemPool::setEntries& ancestors,
                                                CFeeRate dust_relay_feerate);

#endif // BITCOIN_POLICY_EPHEMERAL_POLICY_H
