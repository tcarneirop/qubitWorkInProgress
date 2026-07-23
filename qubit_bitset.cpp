
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <cmath>
#include <omp.h>
#include <sys/time.h>
#include <math.h>
#include <tgmath.h>
#include <vector>
#include <limits.h>
#include <iostream>
#include <regex>
#include <fstream>
#include <cstdint>
#include <numeric>
#include <omp.h>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>

int ALBATROZ[256] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1,
    1, 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 2,
    2, 1, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 2, 3,
    3, 2, 1, 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 2, 3, 4,
    4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 2, 3, 4, 5,
    5, 4, 3, 2, 1, 0, 1, 2, 3, 2, 1, 2, 3, 4, 5, 6,
    6, 5, 4, 3, 2, 1, 0, 1, 2, 1, 2, 3, 4, 5, 6, 7,
    7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8,
    8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7,
    7, 6, 5, 4, 3, 2, 1, 2, 1, 0, 1, 2, 3, 4, 5, 6,
    6, 5, 4, 3, 2, 1, 2, 3, 2, 1, 0, 1, 2, 3, 4, 5,
    5, 4, 3, 2, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4,
    4, 3, 2, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0, 1, 2, 3,
    3, 2, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0, 1, 2,
    2, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1,
    1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0};


constexpr int DIST_SIZE = 5;
int dist[DIST_SIZE * DIST_SIZE] = {
    0, 1, 2, 3, 4,
    1, 0, 1, 2, 3,
    2, 1, 0, 1, 2,
    3, 2, 1, 0, 1,
    4, 3, 2, 1, 0
};


#ifndef MAX_BOARDSIZE
#define MAX_BOARDSIZE 32
#endif

#define MAX_ELEMENTS 100000

#define MIN_BOARDSIZE 2
#define H_TOL 1e-5f

 using Clock = std::chrono::steady_clock;

struct RoutingResult
{
    int num_gates;
    int depth;
};


//@todo: do you access directly the struct like this?
/// For instance, you are using the struct as a global variable? this might be very bad for distributed/parallel
struct PaPrep
{
    int q_a;                // phys_to_logical[p_a]
    int partner_a;          // front_partner[q_a], always != -1
    int p_partner_a;        // mapping[partner_a]
    int dist_a_partner_a;   // dist[p_a * N + p_partner_a]  (pre-swap F term for q_a)
    int e_cnt_a;            // E_adj_count[q_a]
    int e_row_a;            // q_a * E_MAX_DEG
    float decay_a;          // decay[p_a]
    float H_base;           // score_F / |F| + weight * score_E / |E|
    float inv_front;        // 1 / |F|
    float inv_ext_weighted; // weight / |E|   (or 0 if |E|==0)
};

struct SharedCtx
{
    std::vector<int> gates_q1;      // [G]
    std::vector<int> gates_q2;      // [G]
    std::vector<int> successor_idx; // [2*G]
    std::vector<int> rp_init;       // [G]  — initial predecessor counts
    const int *dist;                // [N*N], row-major, not owned
    std::vector<int> phys_nbr_off;  // [N+1] — CSR offsets into phys_nbr_list
    std::vector<int> phys_nbr_list; // [sum of degrees] — flat device adjacency

    std::vector<int> front_sorted_init;  // [front_init_count]
    std::vector<int> front_partner_init; // [n]
    int front_init_count;

    int G;                 // number of gates
    int n;                 // number of logical qubits
    int N;                 // number of physical qubits
    int max_deg;           // max node degree in the device coupling graph
    int E_MAX_DEG;         // per-qubit degree cap in the extended set (= 32)
    int release_threshold; // release-valve threshold (max(4n, 20))
    int pending_cap;       // pending-swap buffer capacity (release_threshold + 2)
    int cand_cap;          // candidate-swap buffer capacity (n*max_deg + 1)
};

struct Scratch
{
    std::vector<int> last_layer;
    std::vector<float> decay;
    std::vector<int> phys_to_logical;
    std::vector<int> front_partner;
    std::vector<int> rp_live;
    std::vector<int> rp_undo; // scratch for decrement-undo in build_extended_front
    std::vector<int> pending_pa;
    std::vector<int> pending_pb;
    std::vector<int> gate_to_remove_gid;
    std::vector<int> path;
    std::vector<int> cand_pa;
    std::vector<int> cand_pb;
    std::vector<int> bfs_to_visit;
    std::vector<int> bfs_visit_now;
    std::vector<int> E_adj;
    std::vector<int> E_adj_count;
    std::vector<int> front_sorted;
    std::vector<char> phys_in_F;
    std::vector<int> phys_in_F_list; // compact ascending list of phys_in_F==1 entries
    std::vector<int> mapping_buf;    // mutable copy of a mapping passed to sabre_route_one

    explicit Scratch(const SharedCtx &ctx)
        : last_layer(ctx.N),
          decay(ctx.N),
          phys_to_logical(ctx.N),
          front_partner(ctx.n),
          rp_live(ctx.G),
          rp_undo(2 * ctx.G), // upper bound: 2 successor-slots per gate
          pending_pa(ctx.pending_cap),
          pending_pb(ctx.pending_cap),
          gate_to_remove_gid(ctx.n),
          path(ctx.N),
          cand_pa(ctx.cand_cap),
          cand_pb(ctx.cand_cap),
          bfs_to_visit(ctx.G),
          bfs_visit_now(ctx.G),
          E_adj((size_t)ctx.n * ctx.E_MAX_DEG),
          E_adj_count(ctx.n),
          front_sorted(ctx.n),
          phys_in_F(ctx.N),
          phys_in_F_list(ctx.N),
          mapping_buf(ctx.n)
    {
    }
};

// remap a single physical-qubit label under a candidate swap (p_a, p_b)
static inline int swap_phys(int p, int p_a, int p_b)
{
    return (p == p_a) ? p_b : ((p == p_b) ? p_a : p);
}

static inline bool gate_lex_less(const int *gates_q1, const int *gates_q2, int a, int b)
{
    return (gates_q1[a] < gates_q1[b]) ||
           (gates_q1[a] == gates_q1[b] && gates_q2[a] < gates_q2[b]);
}

static inline float Hdecay_relative(
    const int *dist, int N,
    const int *E_adj,
    const int *E_adj_count,
    int E_MAX_DEG,
    const int *mapping,
    const int *phys_to_logical,
    const int *front_partner,
    int p_a, int p_b,
    const float *decay,
    const PaPrep &pp)
{
    const int q_a = pp.q_a;
    const int q_b = phys_to_logical[p_b]; // may be -1 (p_b may be an unmapped physical qubit)

    // ---- delta for the basic (front-layer) component ----
    // q_a's F gate (all F gates are 2Q at swap-selection time).
    // p_partner_a is precomputed; its post-swap physical is inline-resolved.
    float delta_F;
    {
        const int p_partner_new = swap_phys(pp.p_partner_a, p_a, p_b);
        delta_F = (float)dist[p_b * N + p_partner_new] - (float)pp.dist_a_partner_a;
    }

    // q_b's F gate, if q_b is mapped and in F (and it's a distinct gate from q_a's)
    if (q_b != -1)
    {
        const int partner_b = front_partner[q_b];
        if (partner_b != -1 && partner_b != q_a)
        {
            const int p_partner = mapping[partner_b];
            const int p_partner_new = swap_phys(p_partner, p_a, p_b);
            delta_F += (float)dist[p_a * N + p_partner_new] - (float)dist[p_b * N + p_partner];
        }
    }

    // ---- delta for the extended (lookahead) component ----
    float delta_E = 0.0f;
    {
        const int cnt = pp.e_cnt_a;
        const int row = pp.e_row_a;
        for (int k = 0; k < cnt; ++k)
        {
            const int other = E_adj[row + k];
            const int p_other = mapping[other];
            const int p_other_new = swap_phys(p_other, p_a, p_b);
            delta_E += (float)dist[p_b * N + p_other_new] - (float)dist[p_a * N + p_other];
        }
    }

    if (q_b != -1)
    {
        const int cnt = E_adj_count[q_b];
        const int row = q_b * E_MAX_DEG;
        for (int k = 0; k < cnt; ++k)
        {
            const int other = E_adj[row + k];
            if (other == q_a)
                continue; // edge (q_a, q_b) already counted from q_a's side
            const int p_other = mapping[other];
            const int p_other_new = swap_phys(p_other, p_a, p_b);
            delta_E += (float)dist[p_a * N + p_other_new] - (float)dist[p_b * N + p_other];
        }
    }

    // ---- assemble H_after * decay_max ----
    // H_base already incorporates the pre-swap score_F/|F| and weight*score_E/|E|;
    // we just add the deltas and multiply by the cached 1/|F| factors.
    const float H_after = pp.H_base + delta_F * pp.inv_front + delta_E * pp.inv_ext_weighted;
    return H_after * std::max(pp.decay_a, decay[p_b]);
}

static inline int front_lower_bound(const int *gates_q1, const int *gates_q2,
                                    const int *front_sorted, int front_count, int gid)
{
    int lo = 0, hi = front_count;
    while (lo < hi)
    {
        const int mid = (lo + hi) / 2;
        if (gate_lex_less(gates_q1, gates_q2, front_sorted[mid], gid))
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static inline void front_insert(const int *gates_q1, const int *gates_q2,
                                int *front_sorted, int &front_count, int gid)
{
    const int pos = front_lower_bound(gates_q1, gates_q2, front_sorted, front_count, gid);
    for (int k = front_count; k > pos; --k)
        front_sorted[k] = front_sorted[k - 1];
    front_sorted[pos] = gid;
    ++front_count;
}

// Caller guarantees gid is currently in the list.
static inline void front_erase(const int *gates_q1, const int *gates_q2,
                               int *front_sorted, int &front_count, int gid)
{
    const int pos = front_lower_bound(gates_q1, gates_q2, front_sorted, front_count, gid);
    for (int k = pos; k + 1 < front_count; ++k)
        front_sorted[k] = front_sorted[k + 1];
    --front_count;
}

// swap the logical qubits sitting on physical qubits p_a and p_b (O(1) via inverse map)
static inline void apply_swap(int *mapping, int *phys_to_logical, int p_a, int p_b)
{
    int q_a = phys_to_logical[p_a];
    int q_b = phys_to_logical[p_b];
    if (q_a != -1)
        mapping[q_a] = p_b;
    if (q_b != -1)
        mapping[q_b] = p_a;
    phys_to_logical[p_a] = q_b;
    phys_to_logical[p_b] = q_a;
}

static inline uint32_t xorshift32(uint32_t &state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void precompute_dag(const std::vector<int> &gates_q1, const std::vector<int> &gates_q2, int n,
                    std::vector<int> &successor_idx, std::vector<int> &required_predecessors)
{
    const int G = (int)gates_q1.size();
    successor_idx.assign(G * 2, -1);
    required_predecessors.assign(G, 0);

    std::vector<int> last_touch_q(n, -1);

    for (int g = 0; g < G; ++g)
    {
        const int q1 = gates_q1[g];
        const int q2 = gates_q2[g];
        //printf("\n\nGATES!!!\n\t%d - %d\n", gates_q1[g], gates_q2[g]);
        const int prev1 = last_touch_q[q1];
        if (prev1 != -1)
        {
            const int slot = (gates_q1[prev1] == q1) ? 0 : 1;
            successor_idx[prev1 * 2 + slot] = g;
            required_predecessors[g]++;
        }
        last_touch_q[q1] = g;

        if (q2 != -1)
        {
            const int prev2 = last_touch_q[q2];
            if (prev2 != -1)
            {
                const int slot = (gates_q1[prev2] == q2) ? 0 : 1;
                successor_idx[prev2 * 2 + slot] = g;
                required_predecessors[g]++;
            }
            last_touch_q[q2] = g;
        }
    }
}

int build_extended_front(const int *gates_q1, const int *gates_q2,
                         const int *successor_idx,
                         int *rp_live, int G,
                         const int *front_sorted, int front_count,
                         int n, int E_MAX_DEG,
                         int *rp_undo,
                         int *to_visit,
                         int *visit_now,
                         int *E_adj,
                         int *E_adj_count)
{
    (void)G; // rp_live bounds no longer referenced; keep parameter for symmetry
    const int ext_cap{20};

    std::fill(E_adj_count, E_adj_count + n, 0);

    int to_visit_n = 0;
    for (int k = 0; k < front_count; ++k)
        to_visit[to_visit_n++] = front_sorted[k];

    int i = 0;
    int visit_now_n = 0;
    int ext_size = 0; // running count of 2Q gates in E
    int undo_n = 0;   // number of decrements recorded for restore

    while (i < to_visit_n && ext_size < ext_cap)
    {
        visit_now[visit_now_n++] = to_visit[i];

        int j = 0;
        while (j < visit_now_n)
        {
            const int g = visit_now[j];

            // When both slots of g point to the same successor (that successor shares both
            // qubits with g), the two per-slot decrements match the per-qubit rp init; the
            // == 0 guard then adds the gate exactly once. Sort by Gate-lex first so iteration
            // order is deterministic.
            int succs[2] = {successor_idx[g * 2 + 0], successor_idx[g * 2 + 1]};
            if (succs[0] != -1 && succs[1] != -1 &&
                gate_lex_less(gates_q1, gates_q2, succs[1], succs[0]))
                std::swap(succs[0], succs[1]);

            for (int ki = 0; ki < 2; ++ki)
            {
                const int s = succs[ki];
                if (s == -1)
                    continue;

                rp_undo[undo_n++] = s;
                if (--rp_live[s] == 0)
                {
                    const int sq2 = gates_q2[s];
                    if (sq2 != -1) // two-qubit gate
                    {
                        const int sq1 = gates_q1[s];

                        E_adj[sq1 * E_MAX_DEG + E_adj_count[sq1]++] = sq2;
                        E_adj[sq2 * E_MAX_DEG + E_adj_count[sq2]++] = sq1;

                        to_visit[to_visit_n++] = s;
                        ++ext_size;
                    }
                    else // single-qubit gate
                    {
                        visit_now[visit_now_n++] = s;
                    }
                }
            }
            j++;
        }
        visit_now_n = 0;
        i++;
    }

    // Restore rp_live to its caller-visible state.
    for (int k = 0; k < undo_n; ++k)
        ++rp_live[rp_undo[k]];

    return ext_size;
}

void sabre_route_one(const SharedCtx &ctx,
                     int *mapping, uint32_t rng_seed,
                     Scratch &sc,
                     int *out_num_gates, int *out_depth)
{
    // --- Unpack ctx as raw pointer / scalar locals for terse body code. ---
    const int *gates_q1 = ctx.gates_q1.data();
    const int *gates_q2 = ctx.gates_q2.data();
    const int *successor_idx = ctx.successor_idx.data();
    const int *rp_init = ctx.rp_init.data();
    const int *dist = ctx.dist;
    const int *phys_nbr_off = ctx.phys_nbr_off.data();
    const int *phys_nbr_list = ctx.phys_nbr_list.data();
    const int G = ctx.G;
    const int n = ctx.n;
    const int N = ctx.N;
    const int E_MAX_DEG = ctx.E_MAX_DEG;
    const int release_threshold = ctx.release_threshold;

    // --- Unpack scratch references (no copy — just aliases for readability). ---
    auto &last_layer = sc.last_layer;
    auto &decay = sc.decay;
    auto &phys_to_logical = sc.phys_to_logical;
    auto &front_partner = sc.front_partner;
    auto &rp_live = sc.rp_live;
    auto &rp_undo = sc.rp_undo;
    auto &pending_pa = sc.pending_pa;
    auto &pending_pb = sc.pending_pb;
    auto &gate_to_remove_gid = sc.gate_to_remove_gid;
    auto &path = sc.path;
    auto &cand_pa = sc.cand_pa;
    auto &cand_pb = sc.cand_pb;
    auto &bfs_to_visit = sc.bfs_to_visit;
    auto &bfs_visit_now = sc.bfs_visit_now;
    auto &E_adj = sc.E_adj;
    auto &E_adj_count = sc.E_adj_count;
    auto &front_sorted = sc.front_sorted;
    auto &phys_in_F = sc.phys_in_F;
    auto &phys_in_F_list = sc.phys_in_F_list;

    int num_gates = 0;
    const float increment = 1e-3f;
    constexpr int DECAY_RESET_PERIOD = 5; // Matches Qiskit LightSABRE: decay
                                          // is reset to 1.0 every 5 swap
                                          // selections (sabre/route.rs).
    int num_search_steps = 0;

    // PRNG state for tie-breaking. xorshift32 requires a non-zero seed.
    uint32_t rng_state = (rng_seed != 0u) ? rng_seed : 1u;

    // Initialize per-call state.
    std::fill(last_layer.begin(), last_layer.end(), -1);
    std::fill(decay.begin(), decay.end(), 1.0f);
    std::fill(phys_to_logical.begin(), phys_to_logical.end(), -1);

    for (int q = 0; q < n; ++q)
        phys_to_logical[mapping[q]] = q;

    // Live predecessor counts start from the DAG's initial counts.
    std::copy(rp_init, rp_init + G, rp_live.begin());

    // Initial front layer + partner map are circuit-only state, cached in ctx.
    // Copy into the per-call buffers rather than rebuilding with an O(G) scan.
    int front_count = ctx.front_init_count;
    std::copy(ctx.front_sorted_init.begin(),
              ctx.front_sorted_init.begin() + front_count,
              front_sorted.begin());
    std::copy(ctx.front_partner_init.begin(),
              ctx.front_partner_init.end(),
              front_partner.begin());

    // Release-valve state (LightSABRE, paper §II.7).
    // Swaps selected since the last routed gate are held pending: applied to `mapping`
    // but not yet committed to last_layer / num_gates. Committed on the next routed
    // gate, or reverted in place if the valve trips.
    int pending_count = 0;

    while (front_count > 0)
    {
        int gate_to_remove_count = 0;
        bool pending_committed = false;

        for (int k = 0; k < front_count; ++k)
        {
            const int g_id = front_sorted[k];
            const int qubit_1 = gates_q1[g_id];
            const int qubit_2 = gates_q2[g_id];

            int phys_qubit_1 = mapping[qubit_1];
            int phys_qubit_2 = (qubit_2 == -1) ? -1 : mapping[qubit_2];

            const bool routable = (qubit_2 == -1) || (dist[phys_qubit_1 * N + phys_qubit_2] == 1);
            if (!routable)
                continue;

            // Commit any pending SABRE swaps before placing this gate so that the
            // per-qubit last_layer reflects them when computing the gate's layer.
            // Each SWAP is counted as 3 in both num_gates and depth: a SwapGate
            // decomposes to CX(a,b)*CX(b,a)*CX(a,b) — three CX in series on the
            // same pair, so the per-qubit last_layer advances by exactly 3 and
            // the gate count grows by 3 per SWAP. This matches what Qiskit's
            // BasisTranslator produces when the translation stage is run.
            if (!pending_committed)
            {
                for (int i = 0; i < pending_count; ++i)
                {
                    const int pa = pending_pa[i], pb = pending_pb[i];
                    int li = std::max(last_layer[pa], last_layer[pb]) + 3;
                    last_layer[pa] = li;
                    last_layer[pb] = li;
                    num_gates += 3;
                }
                pending_count = 0;
                pending_committed = true;
            }

            if (qubit_2 == -1)
            {
                last_layer[phys_qubit_1] += 1;
                ++num_gates;
            }
            else
            {
                int li = std::max(last_layer[phys_qubit_1], last_layer[phys_qubit_2]) + 1;
                last_layer[phys_qubit_1] = li;
                last_layer[phys_qubit_2] = li;
                ++num_gates;
            }

            gate_to_remove_gid[gate_to_remove_count++] = g_id;
        }

        if (gate_to_remove_count > 0)
        {
            // Reset decay on every gate-routing event, mirroring Qiskit
            // LightSABRE (route.rs: `state.decay.fill(1.)` after each
            // `update_route` when the decay heuristic is enabled). This is
            // in addition to the every-DECAY_RESET_PERIOD-swaps periodic
            // reset further down. The effect on bias is small in practice
            // (decay is already bounded by the periodic reset), but keeping
            // both resets matches Qiskit's algorithm exactly.
            std::fill(decay.begin(), decay.end(), 1.0f);
            num_search_steps = 0;

            for (int i = 0; i < gate_to_remove_count; ++i)
            {
                const int g_id = gate_to_remove_gid[i];
                const int gq1 = gates_q1[g_id];
                const int gq2 = gates_q2[g_id];
                front_erase(gates_q1, gates_q2, front_sorted.data(), front_count, g_id);

                if (gq2 != -1)
                {
                    front_partner[gq1] = -1;
                    front_partner[gq2] = -1;
                }

                // Process successors via the precomputed DAG. When both slots of g_id point
                // to the same successor (the successor shares both qubits with g_id), the
                // two per-slot decrements match the per-qubit rp init; the == 0 guard adds
                // the gate exactly once. Sort by Gate-lex first for deterministic iteration.
                int succs[2] = {successor_idx[g_id * 2 + 0], successor_idx[g_id * 2 + 1]};
                if (succs[0] != -1 && succs[1] != -1 &&
                    gate_lex_less(gates_q1, gates_q2, succs[1], succs[0]))
                    std::swap(succs[0], succs[1]);

                for (int ki = 0; ki < 2; ++ki)
                {
                    const int s = succs[ki];
                    if (s == -1)
                        continue;
                    if (--rp_live[s] == 0)
                    {
                        const int nq1 = gates_q1[s];
                        const int nq2 = gates_q2[s];
                        front_insert(gates_q1, gates_q2, front_sorted.data(), front_count, s);
                        if (nq2 != -1)
                        {
                            front_partner[nq1] = nq2;
                            front_partner[nq2] = nq1;
                        }
                    }
                }
            }
        }
        else if (pending_count > release_threshold)
        {
            // ---------- RELEASE VALVE (LightSABRE §II.7) ----------
            // 1. Revert pending swaps (each swap is self-inverse; reverse order cancels).
            for (int i = pending_count - 1; i >= 0; --i)
                apply_swap(mapping, phys_to_logical.data(), pending_pa[i], pending_pb[i]);
            pending_count = 0;

            // 2. Pick the F gate with the smallest current physical distance.
            int target_q1 = -1, target_q2 = -1;
            int min_d = std::numeric_limits<int>::max();
            for (int k = 0; k < front_count; ++k)
            {
                const int g_id = front_sorted[k];
                const int q1 = gates_q1[g_id];
                const int q2 = gates_q2[g_id];
                if (q2 == -1)
                    continue;
                const int d = dist[mapping[q1] * N + mapping[q2]];
                if (d < min_d)
                {
                    min_d = d;
                    target_q1 = q1;
                    target_q2 = q2;
                }
            }

            // 3. Reconstruct a shortest path via greedy distance-decrement.
            // Iterate only the actual neighbours of `cur` (CSR adjacency) rather
            // than scanning all N physical qubits per step — for heavy-hex / IBM-
            // style topologies the degree is ~3, so this is a big constant-factor
            // win when the release valve fires.
            const int p_start = mapping[target_q1];
            const int p_end = mapping[target_q2];
            const int d = min_d;
            int path_len = 0;
            path[path_len++] = p_start;
            {
                int cur = p_start;
                for (int step = 0; step < d; ++step)
                {
                    const int remaining = d - step - 1;
                    const int nbr_lo = phys_nbr_off[cur];
                    const int nbr_hi = phys_nbr_off[cur + 1];
                    for (int k = nbr_lo; k < nbr_hi; ++k)
                    {
                        const int nxt = phys_nbr_list[k];
                        if (dist[nxt * N + p_end] == remaining)
                        {
                            path[path_len++] = nxt;
                            cur = nxt;
                            break;
                        }
                    }
                }
            }

            // 4. Apply swaps from both ends of the path so the two operands meet in the middle.
            //    These swaps are final (committed immediately via last_layer / num_gates).
            const int k = (d - 1) / 2; // q1 at p_start takes k forward steps; q2 at p_end takes d-1-k
            for (int i = 0; i < k; ++i)
            {
                int pa = path[i], pb = path[i + 1];
                apply_swap(mapping, phys_to_logical.data(), pa, pb);
                // SWAP = 3 layers / 3 gates (CX·CX·CX in series on the same pair).
                int li = std::max(last_layer[pa], last_layer[pb]) + 3;
                last_layer[pa] = li;
                last_layer[pb] = li;
                num_gates += 3;
            }
            for (int j = 0; j < d - 1 - k; ++j)
            {
                int idx = d - j;
                int pa = path[idx], pb = path[idx - 1];
                apply_swap(mapping, phys_to_logical.data(), pa, pb);
                // SWAP = 3 layers / 3 gates (CX·CX·CX in series on the same pair).
                int li = std::max(last_layer[pa], last_layer[pb]) + 3;
                last_layer[pa] = li;
                last_layer[pb] = li;
                num_gates += 3;
            }

            // target_gate is now at distance 1; next outer iteration will route it.
            std::fill(decay.begin(), decay.end(), 1.0f);
            num_search_steps = 0;
        }
        else
        {
            // ---------- NORMAL SWAP SELECTION (LightSABRE relative scoring) ----------
            const int ext_size = build_extended_front(
                gates_q1, gates_q2, successor_idx, rp_live.data(), G,
                front_sorted.data(), front_count, n, E_MAX_DEG,
                rp_undo.data(), bfs_to_visit.data(), bfs_visit_now.data(),
                E_adj.data(), E_adj_count.data());
            const int front_size = front_count;

            float score_F = 0.0f;
            for (int k = 0; k < front_count; ++k)
            {
                const int g_id = front_sorted[k];
                score_F += (float)dist[mapping[gates_q1[g_id]] * N + mapping[gates_q2[g_id]]];
            }
            // score_E: skip qubits with no extended-set partners. Hoisting
            // mapping[q] out of the inner loop avoids the cache-miss chain on
            // qubits that wouldn't contribute anyway.
            float score_E = 0.0f;
            for (int q = 0; q < n; ++q)
            {
                const int cnt = E_adj_count[q];
                if (cnt == 0)
                    continue;
                const int mq = mapping[q];
                const int row = q * E_MAX_DEG;
                for (int kk = 0; kk < cnt; ++kk)
                    score_E += (float)dist[mq * N + mapping[E_adj[row + kk]]];
            }
            score_E *= 0.5f; // each edge counted twice in E_adj

            // Precompute normalisers (H_base + inv_front + inv_ext_weighted are
            // constant across the entire swap-candidate loop).
            constexpr float weight = 0.5f;
            const float inv_front = 1.0f / (float)front_size;
            const float inv_ext_weighted = (ext_size > 0) ? (weight / (float)ext_size) : 0.0f;
            const float H_base = score_F * inv_front + score_E * inv_ext_weighted;

            float Hmin = std::numeric_limits<float>::max();
            int cand_count = 0;

            // Build phys_in_F_list in ascending physical-qubit order in the same
            // pass that marks phys_in_F. The ascending order preserves
            // (seed, mapping)-reproducibility of the tie-break RNG. Iterating
            // this compact list avoids the O(N) outer scan on large backends.
            std::fill(phys_in_F.begin(), phys_in_F.end(), (char)0);
            for (int k = 0; k < front_count; ++k)
            {
                const int g_id = front_sorted[k];
                phys_in_F[mapping[gates_q1[g_id]]] = 1;
                phys_in_F[mapping[gates_q2[g_id]]] = 1;
            }
            int phys_in_F_count = 0;
            for (int p = 0; p < N; ++p)
                if (phys_in_F[p])
                    phys_in_F_list[phys_in_F_count++] = p;

            for (int ia = 0; ia < phys_in_F_count; ++ia)
            {
                const int p_a = phys_in_F_list[ia];

                // Precompute everything the inner p_b loop needs that only
                // depends on p_a (and ext-loop invariants).
                PaPrep pp;
                pp.q_a = phys_to_logical[p_a];
                pp.partner_a = front_partner[pp.q_a];
                pp.p_partner_a = mapping[pp.partner_a];
                pp.dist_a_partner_a = dist[p_a * N + pp.p_partner_a];
                pp.e_cnt_a = E_adj_count[pp.q_a];
                pp.e_row_a = pp.q_a * E_MAX_DEG;
                pp.decay_a = decay[p_a];
                pp.H_base = H_base;
                pp.inv_front = inv_front;
                pp.inv_ext_weighted = inv_ext_weighted;

                const int nbr_lo = phys_nbr_off[p_a];
                const int nbr_hi = phys_nbr_off[p_a + 1];
                for (int k = nbr_lo; k < nbr_hi; ++k)
                {
                    const int p_b = phys_nbr_list[k];

                    if (p_b < p_a && phys_in_F[p_b])
                        continue;

                    float H = Hdecay_relative(dist, N,
                                              E_adj.data(), E_adj_count.data(), E_MAX_DEG,
                                              mapping, phys_to_logical.data(), front_partner.data(),
                                              p_a, p_b, decay.data(), pp);

                    if (H < Hmin - H_TOL)
                    {
                        cand_pa[0] = p_a;
                        cand_pb[0] = p_b;
                        cand_count = 1;
                        Hmin = H;
                    }
                    else if (std::fabs(H - Hmin) <= H_TOL)
                    {
                        cand_pa[cand_count] = p_a;
                        cand_pb[cand_count] = p_b;
                        ++cand_count;
                    }
                }
            }

            const int pick = (int)(xorshift32(rng_state) % (uint32_t)cand_count);
            const int swap_idx_1 = cand_pa[pick];
            const int swap_idx_2 = cand_pb[pick];

            ++num_search_steps;
            if (num_search_steps >= DECAY_RESET_PERIOD)
            {
                std::fill(decay.begin(), decay.end(), 1.0f);
                num_search_steps = 0;
            }
            else
            {
                decay[swap_idx_1] += increment;
                decay[swap_idx_2] += increment;
            }

            // Apply the swap to the current mapping and queue it; commit to last_layer
            // and num_gates only when the next gate actually routes.
            apply_swap(mapping, phys_to_logical.data(), swap_idx_1, swap_idx_2);
            pending_pa[pending_count] = swap_idx_1;
            pending_pb[pending_count] = swap_idx_2;
            ++pending_count;
        }
    }

    int depth = 0;
    for (int i = 0; i < N; ++i)
        if (last_layer[i] + 1 > depth)
            depth = last_layer[i] + 1;

    *out_num_gates = num_gates;
    *out_depth = depth;
}


                  
std::vector<RoutingResult> SABRE_routing_many(
    const int *gates_flat, int num_gates_in,
    const int *dist, int N,
    int n, int P, const int *mappings_data, uint32_t base_seed,
    int num_trials, int num_threads)
{

    SharedCtx ctx;
    ctx.dist = dist;
    ctx.N = N;
    ctx.n = n;
    ctx.E_MAX_DEG = 32; // safely exceeds ext_cap (20)

    // Device adjacency in CSR form (dist[i,j]==1 iff there is an edge i—j).
    ctx.phys_nbr_off.assign(N + 1, 0);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            if (i != j && dist[i * N + j] == 1)
                ctx.phys_nbr_off[i + 1]++;
    for (int i = 0; i < N; ++i)
        ctx.phys_nbr_off[i + 1] += ctx.phys_nbr_off[i];

    ctx.phys_nbr_list.resize(ctx.phys_nbr_off[N]);
    {
        std::vector<int> cursor(ctx.phys_nbr_off.begin(), ctx.phys_nbr_off.begin() + N);
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                if (i != j && dist[i * N + j] == 1)
                    ctx.phys_nbr_list[cursor[i]++] = j;
    }

    ctx.max_deg = 0;
    for (int p = 0; p < N; ++p)
    {
        const int deg = ctx.phys_nbr_off[p + 1] - ctx.phys_nbr_off[p];
        if (deg > ctx.max_deg)
            ctx.max_deg = deg;
    }

    ctx.G = num_gates_in;
    ctx.gates_q1.resize(num_gates_in);
    ctx.gates_q2.resize(num_gates_in);
    for (int i = 0; i < num_gates_in; ++i)
    {
        ctx.gates_q1[i] = gates_flat[2 * i];
        ctx.gates_q2[i] = gates_flat[2 * i + 1];
    }

    precompute_dag(ctx.gates_q1, ctx.gates_q2, n, ctx.successor_idx, ctx.rp_init);

    ctx.release_threshold = 10 * n;
    ctx.pending_cap = ctx.release_threshold + 2;
    ctx.cand_cap = n * ctx.max_deg + 1;

    ctx.front_sorted_init.resize(n);
    ctx.front_partner_init.assign(n, -1);
    ctx.front_init_count = 0;
    for (int g = 0; g < ctx.G; ++g)
    {
        if (ctx.rp_init[g] == 0)
            front_insert(ctx.gates_q1.data(), ctx.gates_q2.data(),
                         ctx.front_sorted_init.data(), ctx.front_init_count, g);
    }
    for (int k = 0; k < ctx.front_init_count; ++k)
    {
        const int g_id = ctx.front_sorted_init[k];
        const int q1 = ctx.gates_q1[g_id];
        const int q2 = ctx.gates_q2[g_id];
        if (q2 != -1)
        {
            ctx.front_partner_init[q1] = q2;
            ctx.front_partner_init[q2] = q1;
        }
    }

    std::vector<RoutingResult> results(P);

    const int max_threads = 1; // Change by Jérôme Rouzé
    const long long total_tasks = (long long)P * (long long)num_trials;
    int nt = (num_threads <= 0) ? max_threads : num_threads;
    if (nt > max_threads)
        nt = max_threads;
    if ((long long)nt > total_tasks)
        nt = (int)total_tasks;
    if (nt < 1)
        nt = 1;

    std::vector<int> trial_results((size_t)P * (size_t)num_trials * 2);

    Scratch sc(ctx);
    for (int p = 0; p < P; ++p)
    {
        for (int t = 0; t < num_trials; ++t)
        {
            std::copy(mappings_data + (size_t)p * n,
                        mappings_data + (size_t)(p + 1) * n,
                        sc.mapping_buf.begin());

            const uint32_t rng_seed =
                base_seed + (uint32_t)p * (uint32_t)num_trials + (uint32_t)t + 1u;

            int num_gates = 0, depth = 0;
            sabre_route_one(ctx, sc.mapping_buf.data(), rng_seed, sc,
                            &num_gates, &depth);

            // Write to this (p, t)'s own slot — no contention, no atomic.
            const size_t base = ((size_t)p * (size_t)num_trials + (size_t)t) * 2;
            trial_results[base + 0] = num_gates;
            trial_results[base + 1] = depth;
        }
    }


    for (int p = 0; p < P; ++p)
    {
        int best_num_gates = std::numeric_limits<int>::max();
        int best_depth = std::numeric_limits<int>::max();
        for (int t = 0; t < num_trials; ++t)
        {
            const size_t base = ((size_t)p * (size_t)num_trials + (size_t)t) * 2;
            const int ng = trial_results[base + 0];
            if (ng < best_num_gates)
            {
                best_num_gates = ng;
                best_depth = trial_results[base + 1];
            }
        }
        results[p] = {best_num_gates, best_depth};
    }

    return results;
}


// ---------------------------------------------------------------------------
// Minimal QASM parser.
// ---------------------------------------------------------------------------
// Reads a QASM file and produces `gates_flat` in the same layout that
// utils.get_circuit_array() emits on the Python side:
//   * flat length 2 * num_gates
//   * for gate i: gates_flat[2*i] = q1, gates_flat[2*i + 1] = q2 (or -1
//     for single-qubit gates)
// Also reads `n` from the `qreg q[n];` declaration.
//
// Kept intentionally simple: assumes a single qreg named `q`, no measurement
// gates in the middle, no OpenQASM 3 syntax. Adequate for the small
// hand-crafted test.qasm circuits used to validate SABRE_routing_many.
struct ParsedCircuit {
    std::vector<int> gates_flat;
    int num_gates = 0;
    int n = 0;
};

static ParsedCircuit parse_qasm(const std::string& filename)
{
    std::ifstream f(filename);
    if (!f) {
        throw std::runtime_error("Cannot open QASM file: " + filename);
    }

    ParsedCircuit pc;
    const std::regex qreg_re (R"(qreg\s+\w+\s*\[\s*(\d+)\s*\])");
    const std::regex qubit_re(R"(q\s*\[\s*(\d+)\s*\])");

    std::string line;
    while (std::getline(f, line)) {
        // Strip end-of-line comment, then leading whitespace.
        if (auto pos = line.find("//"); pos != std::string::npos)
            line = line.substr(0, pos);
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip QASM header / classical-only / boundary lines.
        if (line.rfind("OPENQASM", 0) == 0) continue;
        if (line.rfind("include",  0) == 0) continue;
        if (line.rfind("barrier",  0) == 0) continue;
        if (line.rfind("measure",  0) == 0) continue;
        if (line.rfind("reset",    0) == 0) continue;
        if (line.rfind("creg",     0) == 0) continue;

        // Read qreg size.
        if (std::smatch m; std::regex_search(line, m, qreg_re)) {
            pc.n = std::stoi(m[1]);
            continue;
        }

        // Anything else is treated as a gate line; collect its q[N] operands.
        std::vector<int> qubits;
        for (auto it = std::sregex_iterator(line.begin(), line.end(), qubit_re);
             it != std::sregex_iterator();
             ++it)
        {
            qubits.push_back(std::stoi((*it)[1]));
        }
        if (qubits.empty()) continue;

        pc.gates_flat.push_back(qubits[0]);
        pc.gates_flat.push_back(qubits.size() >= 2 ? qubits[1] : -1);
        pc.num_gates++;
    }

    if (pc.n == 0) {
        throw std::runtime_error("No qreg declaration found in " + filename);
    }
    return pc;
}



inline unsigned long long factorial(unsigned long long n)
{
    return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}


void check(const long long m, const long long d, const unsigned long long nsols)
{
    unsigned long long mfat   =   factorial((unsigned long long)m);
    unsigned long long mmdfat =   factorial((unsigned long long)(m - d));

    if (mfat / (mmdfat) != nsols)
    {
        printf("\n############ ERROR - WRONG NUMBER OF SOLS #############\n");
        exit(1);
    }
    else
    {
        printf("\n############ %llu NUM SOLS OK #############\n", nsols);
    }
}




std::vector<int> SERIAL_search_64(int *PHYSIC_MACHINE, int *circuit,  const int num_gates, const long long physic, const long long logic)
{

    unsigned int depth = 0U;
    int mapping[MAX_BOARDSIZE];
    long long aQueenBitCol[MAX_BOARDSIZE];
    long long aStack[MAX_BOARDSIZE];
    unsigned long long numSolutions = 0ULL;

    std::vector<RoutingResult> results;
    std::vector<int> best_mapping;

    long long int *pnStack;

    long long int pnStackPos = 0LLU;

    long long numrows = 0LL;
    unsigned long long lsb;
    unsigned long long bitfield;
    long long i;

    long long mask = (1LL << physic) - 1LL;

    unsigned long long tree_size = 0ULL;
    /* Initialize stack */
    aStack[0] = -1LL; /* set sentinel -- signifies end of stack */

    bitfield = 0ULL;

    bitfield = (1LL << physic) - 1LL;
    pnStack = aStack + 1LL;

    pnStackPos++;

    mapping[0] = 0;
    aQueenBitCol[0] =  0LL;

    int best_depth = INT_MAX;
    int best_num_gates = INT_MAX;


    for (;;)
    {

        lsb = -((signed long long)bitfield) & bitfield;
        if (0ULL == bitfield)
        {

            bitfield = *--pnStack;
            pnStackPos--;

            if (pnStack == aStack)
            {
                break;
            }

            --numrows;
            continue;
        }

        bitfield &= ~lsb;
        mapping[numrows] = (int)(63 - __builtin_clzll(lsb));

        if (numrows < logic)
        {
            long long n = numrows++;
            aQueenBitCol[numrows] = aQueenBitCol[n] | lsb;

            pnStackPos++;

            *pnStack++ = bitfield;

            bitfield = mask & ~(aQueenBitCol[numrows]);

            ++tree_size;

            if (numrows == logic)
            {

                ++numSolutions;
                //what should we do here with parameters for enumeration?
                //remove 
                results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic,logic, 1, mapping, 1 ,1, 1);
                #ifdef VERBOSE
                std::cout<<"results[0].depth: "<< results[0].depth<<"\n";
                std::cout<<"results[0].num_gates: "<< results[0].num_gates<<"\n";
                #endif
                if(results[0].depth<best_depth){
                    std::cout<<"\nNew solution found: \n\tSolution: "<< numSolutions<<", From "<<best_depth<<" to "<<results[0].depth<<"\n";
                    best_depth = results[0].depth;
                    best_num_gates = results[0].num_gates;
                    best_mapping.resize(logic);
                    std::copy(mapping, mapping + logic, best_mapping.begin());
                    for(auto m: best_mapping)
                        std::cout<<m<<" ";
                    std::cout<<"\n";
                }
               // exit(1);

            }

            continue;
        }
        else
        {

            bitfield = *--pnStack;
            pnStackPos--;
            --numrows;
            continue;
        }
    }

#ifdef CHECK
    check(physic, logic, numSolutions);
#endif

    std::cout<<"############# BEST SOL: ##################\n";
    std::cout<<"results[0].depth: "<< best_depth<<"\n";
    std::cout<<"results[0].num_gates: "<< best_num_gates<<"\n";
    std::cout<<"Best mapping:\n";
    for(auto m: best_mapping)
        std::cout<<"  "<<m<<" ";
    std::cout<<std::endl;

    return best_mapping;
}


typedef struct subproblem_t{
    int mapping[11];
    long long  aQueenBitCol; 
}Subproblem;




unsigned long long  mcore_final_search_64(int *PHYSIC_MACHINE, int *circuit,  const int num_gates, const long long physic, 
    const long long logic,const Subproblem* subproblem_pool, 
    const long long cutoff_depth, 
    int* shared_best_depth, 
    int *shared_best_num_gates, 
    int *shared_best_mapping, 
    unsigned long long *shared_sols_counter,
    const int NUMBER_OF_SABRE_RUNS, Clock::time_point start, 
    std::vector<unsigned long long> &number_of_sols)
{


    unsigned int depth = 0U;
    int mapping[MAX_BOARDSIZE];
    long long aQueenBitCol[MAX_BOARDSIZE];
    long long aStack[MAX_BOARDSIZE];
    unsigned long long numSolutions = 0ULL;

    int local_best_depth;
    int local_best_num_gates;

    std::vector<RoutingResult> results;
    std::vector<int> best_mapping;

    long long int *pnStack;

    long long int pnStackPos = 0LLU;

    long long numrows = cutoff_depth; //because we are doing the final search
    unsigned long long lsb;
    unsigned long long bitfield;
    long long i;

    long long mask = (1LL << physic) - 1LL;

    unsigned long long num_sabres = 0ULL;
    

    /* Initialize stack */
    aStack[0] = -1LL; /* set sentinel -- signifies end of stack */

    bitfield = 0ULL;

    bitfield = (1LL << physic) - 1LL;
    pnStack = aStack + 1LL;

    pnStackPos++;

    //starting the search from where we stopped
    aQueenBitCol[numrows] =  subproblem_pool->aQueenBitCol; 
    memcpy(mapping, subproblem_pool->mapping, cutoff_depth * sizeof(int));
    bitfield = mask & ~(aQueenBitCol[numrows]);    

    for (;;)
    {

        lsb = -((signed long long)bitfield) & bitfield;
        if (0ULL == bitfield)
        {

            bitfield = *--pnStack;
            pnStackPos--;

            if (pnStack == aStack)
            {
                break;
            }

            --numrows;
            continue;
        }

        bitfield &= ~lsb;
        mapping[numrows] = (int)(63 - __builtin_clzll(lsb));

        if (numrows < logic)
        {
            long long n = numrows++;
            aQueenBitCol[numrows] = aQueenBitCol[n] | lsb;

            pnStackPos++;

            *pnStack++ = bitfield;

            bitfield = mask & ~(aQueenBitCol[numrows]);


            if (numrows == logic) /////// IT IS A SOLUTION!
            {

                ++numSolutions;
                
                #ifdef VERBOSE
                for(int i = 0; i<logic;++i){
                    printf("\nSolution number: %llu - \n\t", numSolutions);
                    printf(" %d", mapping[i]);
                }
                #endif
                
               
            
                #ifdef SABRE
                results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic,logic, 1, mapping, 1 , NUMBER_OF_SABRE_RUNS, 1);
                ++num_sabres;
                
                number_of_sols[results[0].depth]++;

                #pragma omp atomic read
                local_best_depth = *shared_best_depth;




                if(results[0].depth<local_best_depth){

                    bool improved = false;
                    #pragma omp critical(check_sol)
                    {
                        local_best_depth = *shared_best_depth;
                        if (*shared_best_depth > results[0].depth){
                            *shared_best_depth = results[0].depth;
                            *shared_best_num_gates = results[0].num_gates;
                            memcpy(shared_best_mapping, mapping, logic * sizeof(int));
                            improved = true;
                        }  
                    }//omp critical

                    if(improved){
                        #pragma omp critical(printsol)
                        {

                        (*shared_sols_counter)++;

                        std::cout<<"\nNew solution found at: "<< std::chrono::duration<double>(Clock::now() - start).count()<< "\n\tSolution: "<< *shared_sols_counter <<", From "<<local_best_depth<<" to "<<results[0].depth<<"\n\tDepth: "<<results[0].depth<<"\n\tNum gates: "<<results[0].num_gates<<"\n\tMapping: ";
                        std::cout<<"[";
                        for(int m = 0;m<logic-1;++m)
                            std::cout<<mapping[m]<<", ";
                        std::cout<<mapping[logic-1]<<"]"<<std::endl;

                        }//critical
                        
                    }
    
                }/// if, new sol found
                #endif //end sabre

            }//a leaf

            continue;
        }
        else
        {

            bitfield = *--pnStack;
            pnStackPos--;
            --numrows;
            continue;
        }
    }

    return num_sabres;
}

unsigned long long partial_search_64( const long long physic, const long long cutoff_depth, unsigned long long *num_subproblems,
    Subproblem *subproblem_pool)
{

    unsigned int depth = 0U;
    int mapping[MAX_BOARDSIZE];
    long long aQueenBitCol[MAX_BOARDSIZE];
    long long aStack[MAX_BOARDSIZE];
    unsigned long long numSolutions = 0ULL;


    std::vector<RoutingResult> results;
    std::vector<int> best_mapping;

    long long int *pnStack;

    long long int pnStackPos = 0LLU;

    long long numrows = 0LL;
    unsigned long long lsb;
    unsigned long long bitfield;
    long long i;

    long long mask = (1LL << physic) - 1LL;

    unsigned long long tree_size = 0ULL;
    
    /* Initialize stack */
    aStack[0] = -1LL; /* set sentinel -- signifies end of stack */

    bitfield = 0ULL;

    bitfield = (1LL << physic) - 1LL;
    pnStack = aStack + 1LL;

    pnStackPos++;

    mapping[0] = 0;
    aQueenBitCol[0] =  0LL;

    int best_depth = INT_MAX;
    int best_num_gates = INT_MAX;


    for (;;)
    {

        lsb = -((signed long long)bitfield) & bitfield;
        if (0ULL == bitfield)
        {

            bitfield = *--pnStack;
            pnStackPos--;

            if (pnStack == aStack)
            {
                break;
            }

            --numrows;
            continue;
        }

        bitfield &= ~lsb;
        mapping[numrows] = (int)(63 - __builtin_clzll(lsb));

        if (numrows < cutoff_depth)
        {
            long long n = numrows++;
            aQueenBitCol[numrows] = aQueenBitCol[n] | lsb;

            pnStackPos++;

            *pnStack++ = bitfield;

            bitfield = mask & ~(aQueenBitCol[numrows]);

            ++tree_size;

            if (numrows == cutoff_depth)
            {
                
                for(int i = 0; i<cutoff_depth;++i)
                    subproblem_pool[numSolutions].mapping[i] = mapping[i];
                    subproblem_pool[numSolutions].aQueenBitCol =  aQueenBitCol[numrows];

                ++numSolutions;
            }

            continue;
        }
        else
        {

            bitfield = *--pnStack;
            pnStackPos--;
            --numrows;
            continue;
        }
    }

    *num_subproblems = numSolutions;
    return tree_size;
}



void call_RANDOM_mcore_search(int *PHYSIC_MACHINE, int *circuit, const int num_gates, const long long physic,  
    const long long logic,  const long long cutoff_depth, int *best_depth, 
    int *best_num_gates,
    int *vec_best_mapping, 
    const float PERCENT,
    const int NUMBER_OF_SABRE_RUNS){
    

    Subproblem *subproblem_pool = (Subproblem*)(malloc(sizeof(Subproblem)*(unsigned)100000000));
    
    unsigned long long num_subproblems = 0ULL;
    unsigned long long num_sols_search = 0ULL;
    unsigned long long mcore_tree_size[num_subproblems];
    unsigned long long mcore_num_sols[num_subproblems];
    unsigned long long total_mcore_num_sols = 0ULL;
    unsigned long long total_mcore_tree_size = 0ULL;
    unsigned long long num_sols = 0ULL;
    unsigned long long shared_sols_counter = 0ULL;

    unsigned long long BIGGEST_SOL = 100000ULL;

   
    std::vector<unsigned long long> number_of_sols_value(100000, 0ULL);

    const Clock::time_point start = Clock::now();

    unsigned long long initial_tree_size = partial_search_64(physic, cutoff_depth, &num_subproblems, subproblem_pool);

    

    /////////////////////////////////////////////////////////////////////////
    const std::size_t sample_size = PERCENT * num_subproblems; 

    std::vector<unsigned long long> values(num_subproblems);
    std::iota(values.begin(), values.end(), 0);

    // Seed from the operating system
    std::random_device rd;
    std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    std::mt19937_64 rng(seed);


    // Random permutation
    std::shuffle(values.begin(), values.end(), rng);

    // Keep only 1%
    values.resize(sample_size);

    std::cout<<"\n######## RANDOM SEARCH ############\n\tWorking with "<< values.size() <<" elements out of "<<num_subproblems<<"\n "; 

    /////////////////////////////////////////////////////////////////////////////

    for(unsigned long long i = 0; i<num_subproblems;++i){
        mcore_num_sols[i] = 0ULL;
        mcore_tree_size[i] = 0ULL;
    }

    printf("\nPartial tree: %llu -- Number of subproblems: %llu \n", initial_tree_size, num_subproblems);
    printf("\n### MCORE Search ###\n\tNumber of subproblems: %lld - Physic: %lld, Logic: %lld, Initial depth: %lld,  Max threads: %d\n", num_subproblems, physic, logic, cutoff_depth, omp_get_max_threads());
    
    #ifdef VERBOSE
    for(unsigned long long subproblem = 0; subproblem<num_subproblems;++subproblem){
        std::cout<<"\nSubproblem: "<<subproblem<<" \n\t";
        for(int l = 0; l<cutoff_depth;++l){
            std::cout<< subproblem_pool[subproblem].mapping[l]<< " - ";
        }
    }
    #endif

    #pragma omp parallel for schedule(runtime) default(none) shared(number_of_sols_value,start,values, best_depth, best_num_gates, vec_best_mapping,shared_sols_counter,num_subproblems,PHYSIC_MACHINE, circuit, num_gates, physic, logic, subproblem_pool, cutoff_depth,NUMBER_OF_SABRE_RUNS) reduction(+:num_sols)
    for(unsigned long long subproblem = 0; subproblem<values.size();++subproblem){
        num_sols += mcore_final_search_64(PHYSIC_MACHINE, circuit, num_gates, physic, logic, subproblem_pool+values[subproblem], 
            cutoff_depth, best_depth, best_num_gates,vec_best_mapping,&shared_sols_counter, NUMBER_OF_SABRE_RUNS,start,number_of_sols_value);
    }

    std::cout<<"\nNUM SOLS: "<< num_sols<<std::endl;
    #ifdef CHECK
    check(physic, logic, num_sols);
    #endif

    std::cout<<"\nElapsed time: "<< std::chrono::duration<double>(Clock::now() - start).count()<<std::endl;

    for(unsigned long long index = 0; index<BIGGEST_SOL;++index){ 
        if(number_of_sols_value[index]>0){
             std::cout<<"Solution Value: "<<index<<"\n\tNumber of mappings: "<<number_of_sols_value[index]<<std::endl;
        }
    }

}////////////////////////////////////////////////



///////@@OBS: I'm doing this, the search in C, because we will use a search in Chapel, which has only a C
///////////// Interoperability layer

/* main routine for N Queens program.*/
int main(int argc, char **argv)
{


    //@todo: we are not goind many, why dont we do routing_sabre_one?

    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0] << " <qasm_file> <n physic gates of the desired machine> <cutoff depth> <percent of the pool> <number_of_sabre_runs> \n";
        return EXIT_FAILURE;
    }


    int nb_logic = 0;
    int flat_circuit_size = 0;
    
    const std::string qasm_file = argv[1];
    ParsedCircuit circuit_flat = parse_qasm(qasm_file);


    int nb_physic = atoi(argv[2]);
    int cutoff_depth = atoi(argv[3]);
    float PERCENT = atof(argv[4]);
    int number_of_sabre_runs = 1;

    std::cout<<"argc: "<<argc<<std::endl;
    
    if(argc == 6){

        number_of_sabre_runs = atoi(argv[5]);

        if(number_of_sabre_runs < 1){
            std::cout<<"####### ERROR ########\n\t"<<"Number of SABRE runs < 1.\n";
            exit(1);
        }
    }

    if(nb_physic<nb_logic){
        std::cout<<"####### ERROR ########\n\t"<<"Number of physic gantes needs to be >= number of logic gates.\n";
        exit(1);
    }



    int *PHYSIC_MACHINE; 
    int best_depth = 0;
    int best_num_gates = 0;
    int best_mapping[MAX_BOARDSIZE];



    std::cout<<"circuit_flat.n: "<<circuit_flat.n<<std::endl;
    std::cout<<"circuit_flat.num_gates:"<<circuit_flat.num_gates<<std::endl;
    std::cout<<"Number of SABRE runs: "<<number_of_sabre_runs<<std::endl;

    nb_logic = circuit_flat.n;
    std::cout<<"---- Physic: "<< (long long)(nb_physic)<<" Logic: "<< (long long)(nb_logic)<<std::endl;


    if(cutoff_depth > nb_logic){
        std::cout<<"####### ERROR ########\n\t"<<"cutoff depth needs to be <= nb_logic."<<std::endl;
        exit(1);
    }


    std::cout<<"########### SANITY TEST ################# "<<"\n";
    
    PHYSIC_MACHINE = ALBATROZ;
    std::vector<int> mapping( nb_logic );
    std::iota(mapping.begin(), mapping.end(), 0);
    for(auto m: mapping)
        std::cout<<m<<" ";
    std::cout<<"\n";
    std::vector<RoutingResult> results = SABRE_routing_many(circuit_flat.gates_flat.data(), circuit_flat.num_gates, PHYSIC_MACHINE , nb_physic,circuit_flat.n, 1, mapping.data(), 1,1, 1);

    std::cout<<"results[0].depth: "<< results[0].depth<<"\n";
    std::cout<<"results[0].num_gates: "<< results[0].num_gates<<"\n";

    std::cout<<"########### SANITY TEST ################# "<<"\n";


    //SERIAL_search_64(PHYSIC_MACHINE, circuit_flat.gates_flat.data(), circuit_flat.num_gates, (long long)nb_physic, (long long)nb_logic);
    //partial_search_64((long long)atoi(argv[2]), (long long)(atoi(argv[3])));

    best_depth = results[0].depth;
    best_num_gates = results[0].num_gates;
    memcpy(best_mapping, mapping.data(), nb_logic * sizeof(int));
    
    call_RANDOM_mcore_search(PHYSIC_MACHINE, circuit_flat.gates_flat.data(), circuit_flat.num_gates, (long long)nb_physic, 
        (long long)nb_logic,(long long)cutoff_depth,&best_depth,&best_num_gates,best_mapping,PERCENT, number_of_sabre_runs);


    return 0;
}
