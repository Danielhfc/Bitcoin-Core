// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txreconciliation.h>

/** Static component of the salt used to compute short txids for transaction reconciliation. */
static const std::string RECON_STATIC_SALT = "Tx Relay Salting";

static uint256 ComputeSalt(uint64_t local_salt, uint64_t remote_salt)
{
    uint64_t salt1 = local_salt, salt2 = remote_salt;
    if (salt1 > salt2) std::swap(salt1, salt2);
    static const auto RECON_SALT_HASHER = TaggedHash(RECON_STATIC_SALT);
    return (CHashWriter(RECON_SALT_HASHER) << salt1 << salt2).GetSHA256();
}

std::tuple<bool, bool, uint32_t, uint64_t> TxReconciliationTracker::SuggestReconciling(const NodeId peer_id, bool inbound)
{
    bool be_recon_requestor, be_recon_responder;
    // Currently reconciliation requests flow only in one direction inbound->outbound.
    if (inbound) {
        be_recon_requestor = false;
        be_recon_responder = true;
    } else {
        be_recon_requestor = true;
        be_recon_responder = false;
    }

    uint32_t recon_version = 1;
    uint64_t m_local_recon_salt(GetRand(UINT64_MAX));
    WITH_LOCK(m_local_salts_mutex, m_local_salts.emplace(peer_id, m_local_recon_salt));

    return std::make_tuple(be_recon_requestor, be_recon_responder, recon_version, m_local_recon_salt);
}

bool TxReconciliationTracker::EnableReconciliationSupport(const NodeId peer_id, bool inbound,
    bool recon_requestor, bool recon_responder, uint32_t recon_version, uint64_t remote_salt,
    size_t outbound_flooders)
{
    // Do not support reconciliation salt/version updates
    LOCK(m_states_mutex);
    auto recon_state = m_states.find(peer_id);
    if (recon_state != m_states.end()) return false;

    if (recon_version != 1) return false;

    // Do not flood through inbound connections which support reconciliation to save bandwidth.
    // Flood only through a limited number of outbound connections.
    bool flood_to = false;
    if (inbound) {
        // We currently don't support reconciliations with inbound peers which
        // don't want to be reconciliation senders (request our sketches),
        // or want to be reconciliation responders (send us their sketches).
        // Just ignore SENDRECON and use normal flooding for transaction relay with them.
        if (!recon_requestor) return false;
        if (recon_responder) return false;
    } else {
        // We currently don't support reconciliations with outbound peers which
        // don't want to be reconciliation responders (send us their sketches),
        // or want to be reconciliation senders (request our sketches).
        // Just ignore SENDRECON and use normal flooding for transaction relay with them.
        if (recon_requestor) return false;
        if (!recon_responder) return false;
        // TODO: Flood only through a limited number of outbound connections.
        flood_to = true;
    }

    // Reconcile with all outbound peers supporting reconciliation (even if we flood to them),
    // to not miss transactions they have for us but won't flood.
    if (recon_responder) {
        LOCK(m_queue_mutex);
        m_queue.push_back(peer_id);
    }

    uint64_t local_peer_salt = WITH_LOCK(m_local_salts_mutex, return m_local_salts.at(peer_id));
    uint256 full_salt = ComputeSalt(local_peer_salt, remote_salt);

    m_states.emplace(peer_id, ReconciliationState(recon_requestor, recon_responder,
                        flood_to, full_salt.GetUint64(0), full_salt.GetUint64(1)));
    return true;
}
