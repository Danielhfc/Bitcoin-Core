// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXRECONCILIATION_H
#define BITCOIN_TXRECONCILIATION_H

#include <minisketch/include/minisketch.h>
#include <net.h>
#include <sync.h>

#include <tuple>
#include <unordered_map>

/** The size of the field, used to compute sketches to reconcile transactions (see BIP-330). */
static constexpr unsigned int RECON_FIELD_SIZE = 32;
/**
 * Allows to infer capacity of a reconciliation sketch based on it's char[] representation,
 * which is necessary to deserealize a received sketch.
 */
static constexpr unsigned int BYTES_PER_SKETCH_CAPACITY = RECON_FIELD_SIZE / 8;
/** Limit sketch capacity to avoid DoS. */
static constexpr uint16_t MAX_SKETCH_CAPACITY = 2 << 12;
/**
* It is possible that if sketch encodes more elements than the capacity, or
* if it is constructed of random bytes, sketch decoding may "succeed",
* but the result will be nonsense (false-positive decoding).
* Given this coef, a false positive probability will be of 1 in 2**coef.
*/
static constexpr unsigned int RECON_FALSE_POSITIVE_COEF = 16;
static_assert(RECON_FALSE_POSITIVE_COEF <= 256,
    "Reducing reconciliation false positives beyond 1 in 2**256 is not supported");
/** Default coefficient used to estimate set difference for tx reconciliation. */
static constexpr double DEFAULT_RECON_Q = 0.02;
/** Used to convert a floating point reconciliation coefficient q to an int for transmission.
  * Specified by BIP-330.
  */
static constexpr uint16_t Q_PRECISION{(2 << 14) - 1};
/**
 * Interval between sending reconciliation request to the same peer.
 * This value allows to reconcile ~100 transactions (7 tx/s * 16s) during normal system operation
 * at capacity. More frequent reconciliations would cause significant constant bandwidth overhead
 * due to reconciliation metadata (sketch sizes etc.), which would nullify the efficiency.
 * Less frequent reconciliations would introduce high transaction relay latency.
 */
static constexpr std::chrono::microseconds RECON_REQUEST_INTERVAL{16s};
/**
 * Interval between responding to peers' reconciliation requests.
 * We don't respond to reconciliation requests right away because that would enable monitoring
 * when we receive transactions (privacy leak).
 */
static constexpr std::chrono::microseconds RECON_RESPONSE_INTERVAL{2s};

/**
 * Represents phase of the current reconciliation round with a peer.
 */
enum ReconciliationPhase {
    RECON_NONE,
    RECON_INIT_REQUESTED,
    RECON_INIT_RESPONDED,
};

/**
 * This struct is used to keep track of the reconciliations with a given peer,
 * and also short transaction IDs for the next reconciliation round.
 * Transaction reconciliation means an efficient synchronization of the known
 * transactions between a pair of peers.
 * One reconciliation round consists of a sequence of messages. The sequence is
 * asymmetrical, there is always a requestor and a responder. At the end of the
 * sequence, nodes are supposed to exchange transactions, so that both of them
 * have all relevant transactions. For more protocol details, refer to BIP-0330.
 */
class ReconciliationState {
    /** Whether this peer will send reconciliation requests. */
    bool m_requestor;

    /** Whether this peer will respond to reconciliation requests. */
    bool m_responder;

    /**
     * Since reconciliation-only approach makes transaction relay
     * significantly slower, we also announce some of the transactions
     * (currently, transactions received from inbound links)
     * to some of the peers:
     * - all pre-reconciliation peers supporting transaction relay;
     * - a limited number of outbound reconciling peers *for which this flag is enabled*.
     * We enable this flag based on whether we have a
     * sufficient number of outbound transaction relay peers.
     * This flooding makes transaction relay across the network faster
     * without introducing high the bandwidth overhead.
     * Transactions announced via flooding should not be added to
     * the reconciliation set.
     */
    bool m_flood_to;

    /**
     * Reconciliation involves computing and transmitting sketches,
     * which is a bandwidth-efficient representation of transaction IDs.
     * Since computing sketches over full txID is too CPU-expensive,
     * they will be computed over shortened IDs instead.
     * These short IDs will be salted so that they are not the same
     * across all pairs of peers, because otherwise it would enable network-wide
     * collisions which may (intentionally or not) halt relay of certain transactions.
     * Both of the peers contribute to the salt.
     */
    const uint64_t m_k0, m_k1;

    /**
     * Computing a set reconciliation sketch involves estimating the difference
     * between sets of transactions on two sides of the connection. More specifically,
     * a sketch capacity is computed as
     * |set_size - local_set_size| + q * (set_size + local_set_size) + c,
     * where c is a small constant, and q is a node+connection-specific coefficient.
     * This coefficient is recomputed by every node based on its previous reconciliations,
     * to better predict future set size differences.
     */
    double m_local_q;

    /**
     * The use of q coefficients is described above (see local_q comment).
     * The value transmitted from the peer with a reconciliation requests is stored here until
     * we respond to that request with a sketch.
     */
    double m_remote_q;

    /**
     * Store all transactions which we would relay to the peer (policy checks passed, etc.)
     * in this set instead of announcing them right away. When reconciliation time comes, we will
     * compute an efficient representation of this set ("sketch") and use it to efficient reconcile
     * this set with a similar set on the other side of the connection.
     */
    std::set<uint256> m_local_set;

    /**
     * Reconciliation sketches are computed over short transaction IDs.
     * This is a cache of these IDs enabling faster lookups of full wtxids,
     * useful when peer will ask for missing transactions by short IDs
     * at the end of a reconciliation round.
     */
    std::map<uint32_t, uint256> m_local_short_id_mapping;

    /**
     * A reconciliation request comes from a peer with a reconciliation set size from their side,
     * which is supposed to help us to estimate set difference size. The value is stored here until
     * we respond to that request with a sketch.
     */
    uint16_t m_remote_set_size;

    /**
     * When a reconciliation request is received, instead of responding to it right away,
     * we schedule a response for later, so that a spy can’t monitor our reconciliation sets.
     */
    std::chrono::microseconds m_next_recon_respond{0};

    /** Keep track of reconciliations with the peer. */
    ReconciliationPhase m_incoming_recon{RECON_NONE};
    ReconciliationPhase m_outgoing_recon{RECON_NONE};

    /**
     * Reconciliation sketches are computed over short transaction IDs.
     * Short IDs are salted with a link-specific constant value.
     */
    uint32_t ComputeShortID(const uint256 wtxid) const
    {
        const uint64_t s = SipHashUint256(m_k0, m_k1, wtxid);
        const uint32_t short_txid = 1 + (s & 0xFFFFFFFF);
        return short_txid;
    }

    void ClearState()
    {
        m_local_short_id_mapping.clear();
    }

public:

    ReconciliationState(bool requestor, bool responder, bool flood_to, uint64_t k0, uint64_t k1) :
        m_requestor(requestor), m_responder(responder), m_flood_to(flood_to),
        m_k0(k0), m_k1(k1), m_local_q(DEFAULT_RECON_Q) {}

    bool IsChosenForFlooding() const
    {
        return m_flood_to;
    }

    bool IsRequestor() const
    {
        return m_requestor;
    }

    bool IsResponder() const
    {
        return m_responder;
    }

    std::vector<uint256> GetLocalSet() const
    {
        return std::vector<uint256>(m_local_set.begin(), m_local_set.end());
    }

    uint16_t GetLocalSetSize() const
    {
        return m_local_set.size();
    }

    uint16_t GetLocalQ() const
    {
        return m_local_q * Q_PRECISION;
    }

    ReconciliationPhase GetIncomingPhase() const
    {
        return m_incoming_recon;
    }

    ReconciliationPhase GetOutgoingPhase() const
    {
        return m_outgoing_recon;
    }

    std::chrono::microseconds GetNextRespond() const
    {
        return m_next_recon_respond;
    }

    void AddToReconSet(const std::vector<uint256>& txs_to_reconcile)
    {
        for (const auto& wtxid: txs_to_reconcile) {
            m_local_set.insert(wtxid);
        }
    }

    void UpdateIncomingPhase(ReconciliationPhase phase)
    {
        assert(m_requestor);
        m_incoming_recon = phase;
    }

    void UpdateOutgoingPhase(ReconciliationPhase phase)
    {
        assert(m_responder);
        m_outgoing_recon = phase;
    }

    void PrepareIncoming(uint16_t remote_set_size, double remote_q, std::chrono::microseconds next_respond)
    {
        assert(m_requestor);
        assert(m_incoming_recon == RECON_NONE);
        assert(m_remote_q >= 0 && m_remote_q <= 2);
        m_remote_q = remote_q;
        m_remote_set_size = remote_set_size;
        m_next_recon_respond = next_respond;
    }

    /**
     * Estimate a capacity of a sketch we will send or use locally (to find set difference)
     * based on the local set size.
     */
    uint16_t EstimateSketchCapacity() const
    {
        const uint16_t set_size_diff = std::abs(uint16_t(m_local_set.size()) - m_remote_set_size);
        const uint16_t min_size = std::min(uint16_t(m_local_set.size()), m_remote_set_size);
        const uint16_t weighted_min_size = m_remote_q * min_size;
        const uint16_t estimated_diff = 1 + weighted_min_size + set_size_diff;
        return minisketch_compute_capacity(RECON_FIELD_SIZE, estimated_diff, RECON_FALSE_POSITIVE_COEF);
    }

    /**
     * Reconciliation involves computing a space-efficient representation of transaction identifiers
     * (a sketch). A sketch has a capacity meaning it allows reconciling at most a certain number
     * of elements (see BIP-330).
     */
    Minisketch ComputeSketch(uint16_t capacity)
    {
        Minisketch sketch;
        // Avoid serializing/sending an empty sketch.
        if (m_local_set.size() == 0 || capacity == 0) return sketch;

        std::vector<uint32_t> short_ids;
        for (const auto& wtxid: m_local_set) {
            uint32_t short_txid = ComputeShortID(wtxid);
            short_ids.push_back(short_txid);
            m_local_short_id_mapping.emplace(short_txid, wtxid);
        }

        capacity = std::min(capacity, MAX_SKETCH_CAPACITY);
        sketch = Minisketch(RECON_FIELD_SIZE, 0, capacity);
        if (sketch) {
            for (const uint32_t short_id: short_ids) {
                sketch.Add(short_id);
            }
        }
        return sketch;
    }

    /**
     * After a reconciliation round passed, transactions missing by our peer are known by short ID.
     * Look up their full wtxid locally to announce them to the peer.
     */
    std::vector<uint256> GetWTXIDsFromShortIDs(const std::vector<uint32_t>& remote_missing_short_ids) const
    {
        std::vector<uint256> remote_missing;
        for (const auto& missing_short_id: remote_missing_short_ids) {
            const auto local_tx = m_local_short_id_mapping.find(missing_short_id);
            if (local_tx != m_local_short_id_mapping.end()) {
                remote_missing.push_back(local_tx->second);
            }
        }
        return remote_missing;
    }

    /**
     * TODO document
     */
    void FinalizeIncomingReconciliation()
    {
        assert(m_requestor);
        ClearState();
    }

    /**
     * TODO document
     */
    void FinalizeOutgoingReconciliation(bool clear_local_set, double updated_q)
    {
        assert(m_responder);
        m_local_q = updated_q;
        if (clear_local_set) m_local_set.clear();
        ClearState();
    }

    /**
     * When during reconciliation we find a set difference successfully (by combining sketches),
     * we want to find which transactions are missing on our and on their side.
     * For those missing on our side, we may only find short IDs.
     */
    std::pair<std::vector<uint32_t>, std::vector<uint256>> GetRelevantIDsFromShortIDs(const std::vector<uint64_t>& diff) const
    {
        std::vector<uint32_t> local_missing;
        std::vector<uint256> remote_missing;
        for (const auto& diff_short_id: diff) {
            const auto local_tx = m_local_short_id_mapping.find(diff_short_id);
            if (local_tx != m_local_short_id_mapping.end()) {
                remote_missing.push_back(local_tx->second);
            } else {
                local_missing.push_back(diff_short_id);
            }
        }
        return std::make_pair(local_missing, remote_missing);
    }
};

/**
 * Used to track reconciliations across all peers.
 */
class TxReconciliationTracker {
    /**
     * Salt used to compute short IDs during transaction reconciliation.
     * Salt is generated randomly per-connection to prevent linking of
     * connections belonging to the same physical node.
     * Also, salts should be different per-connection to prevent halting
     * of relay of particular transactions due to collisions in short IDs.
     */
    Mutex m_local_salts_mutex;
    std::unordered_map<NodeId, uint64_t> m_local_salts GUARDED_BY(m_local_salts_mutex);

    /**
     * Used to keep track of ongoing reconciliations (or lack of them) per peer.
     */
    Mutex m_states_mutex;
    std::unordered_map<NodeId, ReconciliationState> m_states GUARDED_BY(m_states_mutex);

    /**
     * Reconciliation should happen with peers in the same order, because the efficiency gain is the
     * highest when reconciliation set difference is predictable. This queue is used to maintain the
     * order of peers chosen for reconciliation.
     */
    Mutex m_queue_mutex;
    std::deque<NodeId> m_queue GUARDED_BY(m_queue_mutex);

    /**
     * Reconciliations are requested periodically:
     * every RECON_REQUEST_INTERVAL seconds we pick a peer from the queue.
     */
    std::chrono::microseconds m_next_recon_request{0};
    void UpdateNextReconRequest(std::chrono::microseconds now) EXCLUSIVE_LOCKS_REQUIRED(m_queue_mutex)
    {
        m_next_recon_request = now + RECON_REQUEST_INTERVAL / m_queue.size();
    }

    /**
     * Used to schedule the next initial response for any pending reconciliation request.
     * Respond to all requests at the same time to prevent transaction possession leak.
     */
    std::chrono::microseconds m_next_recon_respond{0};
    std::chrono::microseconds NextReconRespond()
    {
        auto current_time = GetTime<std::chrono::microseconds>();
        if (m_next_recon_respond < current_time) {
            m_next_recon_respond = current_time + RECON_RESPONSE_INTERVAL;
        }
        return m_next_recon_respond;
    }

    public:

    TxReconciliationTracker() {};

    /**
     * TODO: document
     */
    std::tuple<bool, bool, uint32_t, uint64_t> SuggestReconciling(const NodeId peer_id, bool inbound);

    /**
     * If a peer was previously initiated for reconciliations, get its current reconciliation state.
     * Note that the returned instance is read-only and modifying it won't alter the actual state.
     */
    bool EnableReconciliationSupport(const NodeId peer_id, bool inbound,
        bool recon_requestor, bool recon_responder, uint32_t recon_version, uint64_t remote_salt,
        size_t outbound_flooders);

    /**
     * If a it's time to request a reconciliation from the peer, this function will return the
     * details of our local state, which should be communicated to the peer so that they better
     * know what we need.
     */
    Optional<std::pair<uint16_t, uint16_t>> MaybeRequestReconciliation(const NodeId peer_id);

    /**
     * Record an (expected) reconciliation request with parameters to respond when time comes. All
     * initial reconciliation responses will be done at the same time to prevent privacy leaks.
     */
    void HandleReconciliationRequest(const NodeId peer_id, uint16_t peer_recon_set_size, uint16_t peer_q);

    /**
     * TODO document
     */
    Optional<std::vector<uint8_t>> MaybeRespondToReconciliationRequest(const NodeId peer_id);

    /**
     * TODO document
     */
    std::vector<uint256> FinalizeIncomingReconciliation(const NodeId peer_id,
        bool recon_result, const std::vector<uint32_t>& ask_shortids);

    /**
     * Received a response to the reconciliation request. May leak tx-related privacy if we announce
     * local transactions right away, in case the peer is strategic about sending sketches to us via
     * different connections (requires attacker to occupy multiple outgoing connections).
     * Returns a response we should send to the peer, and the transactions we should announce.
     */
    Optional<std::tuple<bool, std::vector<uint32_t>, std::vector<uint256>>> HandleSketch(
        const NodeId peer_id, int common_version, std::vector<uint8_t>& skdata);

    Optional<ReconciliationState> GetPeerState(const NodeId peer_id) const
    {
        // This does not compile if this function is marked const. Not sure how to fix this.
        // LOCK(m_states_mutex);
        auto recon_state = m_states.find(peer_id);
        if (recon_state != m_states.end()) {
            return recon_state->second;
        } else {
            return nullopt;
        }
    }

    void StoreTxsToAnnounce(const NodeId peer_id, const std::vector<uint256>& txs_to_reconcile)
    {
        LOCK(m_states_mutex);
        auto recon_state = m_states.find(peer_id);
        assert(recon_state != m_states.end());
        recon_state->second.AddToReconSet(txs_to_reconcile);
    }

    void RemovePeer(const NodeId peer_id)
    {
        LOCK(m_queue_mutex);
        m_queue.erase(std::remove(m_queue.begin(), m_queue.end(), peer_id), m_queue.end());
        LOCK(m_local_salts_mutex);
        m_local_salts.erase(peer_id);
        LOCK(m_states_mutex);
        m_states.erase(peer_id);
    }
};

#endif // BITCOIN_TXRECONCILIATION_H
