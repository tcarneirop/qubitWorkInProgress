#ifndef GRASP_HPP
#define GRASP_HPP

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <utility>
#include <vector>
#include <tuple> 
#include <numeric>
#include <chrono>
#include <iostream>
#include <omp.h>
#include <random>
#include <limits>
#include <stdexcept>
#include <iostream>

#include "qasm_parser.hpp"
#include "grasp_graph.hpp"
#include "routing_core.hpp"

using DistanceMatrix = std::vector<std::vector<double>>;


//let's pass a function as an argument
using CostFunction = std::function<double(
    const ParsedCircuit&,
    const std::vector<int>&,
    const DistanceMatrix&
)>;

using LocalCostFunction = std::function<std::pair<int, int>(
    const ParsedCircuit&,
    const std::vector<int>&,
    const DistanceMatrix&
)>;

using FinalCostFunction = std::function<int(
    const ParsedCircuit&,
    const std::vector<int>&
)>;



inline std::pair<int, int> fitness_cpp_swap(
    const ParsedCircuit& circuit,
    const std::vector<int>& mapping,
    const DistanceMatrix& Dist)
{
    const int N = static_cast<int>(Dist.size());

    // Convert distance matrix to a flat integer array
    std::vector<int> dist_flat;
    dist_flat.reserve(N * N);

    for (const auto& row : Dist)
        for (double value : row)
            dist_flat.push_back(static_cast<int>(value));

    // Execute routing for one mapping
    const auto results = SABRE_routing_many(
        circuit.gates_flat.data(),
        circuit.num_gates,
        dist_flat.data(),
        N,
        circuit.n,
        1,
        mapping.data(),
        42,
        1,
        0
    );

    return {results[0].depth, results[0].num_gates};
}

inline double fitness_cpp(
    const ParsedCircuit& circuit,
    const std::vector<int>& mapping,
    const DistanceMatrix& Dist)
{
    const int N = static_cast<int>(Dist.size());

    // Convert the distance matrix to a flat integer array
    std::vector<int> dist_flat;
    dist_flat.reserve(N * N);

    for (const auto& row : Dist)
        for (double value : row)
            dist_flat.push_back(static_cast<int>(value));

    // Execute routing for one mapping
    const auto results = SABRE_routing_many(
        circuit.gates_flat.data(),
        circuit.num_gates,
        dist_flat.data(),
        N,
        circuit.n,
        1,
        mapping.data(),
        42,
        1,
        0
    );

    return static_cast<double>(results[0].depth);
}


std::vector<int> GRASPbuild(
    const ParsedCircuit& circuit,
    const DistanceMatrix& Dist,
    const CostFunction& cost_func,
    double alpha,
    int task_id,
    std::mt19937& rng)
{
    const int nb_qubit_circ = circuit.n;
    const int nb_qubit_back = static_cast<int>(Dist.size());

    const auto Neighborhood_circ = get_neighborhood_circ(circuit);

    std::vector<std::set<int>> Neighborhood_back(nb_qubit_back);

    for (int i = 0; i < nb_qubit_back; ++i)
        for (int j = 0; j < nb_qubit_back; ++j)
            if (Dist[i][j] == 1)
                Neighborhood_back[i].insert(j);

    // Initial logical qubit
    const auto priority = get_init_qubit_list(circuit);

    std::vector<int> keep_qubits = {
        priority[task_id % nb_qubit_circ]
    };

    // Initial physical qubit
    const std::vector<int> initial_positions =
        (nb_qubit_back == 127)
        ? std::vector<int>{62, 63, 64}
        : std::vector<int>{3, 4, 11, 12};

    std::uniform_int_distribution<size_t> initial_dist(
        0, initial_positions.size() - 1
    );

    std::vector<int> mapping = {
        initial_positions[initial_dist(rng)]
    };

    for (int i = 0; i < nb_qubit_circ - 1; ++i) {

        std::set<int> neighbor_circ;
        std::set<int> neighbor_back;

        for (int q : keep_qubits)
            for (int x : Neighborhood_circ[q])
                neighbor_circ.insert(x);

        for (int q : keep_qubits)
            neighbor_circ.erase(q);

        for (int p : mapping)
            for (int x : Neighborhood_back[p])
                neighbor_back.insert(x);

        for (int p : mapping)
            neighbor_back.erase(p);

        // Evaluate candidate pairs
        std::map<std::pair<int, int>, double> pairs;

        for (int q : neighbor_circ) {
            for (int p : neighbor_back) {

                std::vector<int> keep_qubits_testing;
                std::vector<int> mapping_testing;

                auto it = std::lower_bound(
                    keep_qubits.begin(),
                    keep_qubits.end(),
                    q
                );

                const size_t j =
                    std::distance(keep_qubits.begin(), it);

                keep_qubits_testing = keep_qubits;
                mapping_testing = mapping;

                keep_qubits_testing.insert(
                    keep_qubits_testing.begin() + j, q
                );

                // Preserve the original Python behavior
                int selected_p = p;

                if (j < keep_qubits.size()) {
                    std::vector<int> candidates(
                        neighbor_back.begin(),
                        neighbor_back.end()
                    );

                    std::uniform_int_distribution<size_t> dist(
                        0, candidates.size() - 1
                    );

                    selected_p = candidates[dist(rng)];
                }

                mapping_testing.insert(
                    mapping_testing.begin() + j,
                    selected_p
                );

                ParsedCircuit partial_circ =
                    filter_circuit_qubits(
                        circuit, keep_qubits_testing
                    );

                pairs[{q, p}] = cost_func(
                    partial_circ, mapping_testing, Dist
                );
            }
        }

        // Greedy randomized choice
        auto minmax = std::minmax_element(
            pairs.begin(),
            pairs.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            }
        );

        const double minval = minmax.first->second;
        const double maxval = minmax.second->second;

        const double threshold =
            minval + alpha * (maxval - minval);

        std::vector<std::pair<int, int>> best_pairs;

        for (const auto& [pair, cost] : pairs)
            if (cost <= threshold)
                best_pairs.push_back(pair);

        std::uniform_int_distribution<size_t> choice(
            0, best_pairs.size() - 1
        );

        const auto [q, p] = best_pairs[choice(rng)];

        // Insert selected logical and physical qubits
        auto it = std::lower_bound(
            keep_qubits.begin(), keep_qubits.end(), q
        );

        const size_t j =
            std::distance(keep_qubits.begin(), it);

        keep_qubits.insert(keep_qubits.begin() + j, q);
        mapping.insert(mapping.begin() + j, p);
    }

    return mapping;
}


std::tuple<std::vector<int>, int, int, bool>
GRASPlocal_search(
    const ParsedCircuit& circuit,
    const std::vector<int>& mapping,
    int cost,
    int nbgate,
    const DistanceMatrix& Dist,
    const LocalCostFunction& cost_func)
{
    std::vector<int> best_neighbor = mapping;
    int best_cost = cost;
    int best_nbgate = nbgate;
    bool change = false;

    // Build the interaction frequency matrix
    const int n = static_cast<int>(mapping.size());
    std::vector<int> frequency_matrix(n * n, 0);

    for (int g = 0; g < circuit.num_gates; ++g) {
        const int i = circuit.gates_flat[2 * g];
        const int j = circuit.gates_flat[2 * g + 1];

        if (j == -1)
            continue;

        ++frequency_matrix[i * n + j];
        ++frequency_matrix[j * n + i];
    }

    // Order pairs by decreasing interaction frequency
    std::vector<int> pair_order;

    for (int i = 0; i < n * n; ++i)
        pair_order.push_back(i);

    std::sort(pair_order.begin(), pair_order.end(),
        [&](int a, int b) {
            return frequency_matrix[a] < frequency_matrix[b];
        });

    std::reverse(pair_order.begin(), pair_order.end());

    // Explore the neighborhood
    for (int index : pair_order) {

        if (frequency_matrix[index] == 0)
            continue;

        const int l0 = index / n;
        const int l1 = index % n;

        if (l0 == l1)
            continue;

        const int q0 = mapping[l0];

        // Physical neighbors of q0
        for (int s = 0; s < static_cast<int>(Dist.size()); ++s) {

            if (Dist[q0][s] != 1)
                continue;

            if (mapping[l1] == s)
                continue;

            std::vector<int> neighbor = mapping;

            auto it = std::find(
                mapping.begin(), mapping.end(), s
            );

            if (it != mapping.end()) {
                const size_t pos =
                    std::distance(mapping.begin(), it);

                neighbor[pos] = mapping[l1];
            }

            neighbor[l1] = s;

            auto [candidate_cost, candidate_nbgate] =
                cost_func(circuit, neighbor, Dist);

            if (candidate_cost < best_cost) {
                best_cost = candidate_cost;
                best_nbgate = candidate_nbgate;
                best_neighbor = neighbor;
                change = true;
            }
            else if (candidate_cost == best_cost &&
                     candidate_nbgate < best_nbgate) {
                best_cost = candidate_cost;
                best_nbgate = candidate_nbgate;
                best_neighbor = neighbor;
                change = true;
            }
        }

        // Stop after the first pair that produces an improvement
        if (change)
            break;
    }

    return {
        best_neighbor,
        best_cost,
        best_nbgate,
        change
    };
}


std::pair<std::vector<int>, int>
GRASPiteration(
    const ParsedCircuit& circuit,
    const DistanceMatrix& Dist,
    const CostFunction& cost_func,
    const LocalCostFunction& local_cost_func,
    const FinalCostFunction& fitnessOG,
    double alpha,
    int task_id,
    std::mt19937& rng)
{
    using Clock = std::chrono::steady_clock;

    // Construction phase
    auto start1 = Clock::now();

    std::vector<int> mapping =
        GRASPbuild(circuit, Dist, cost_func, alpha, task_id, rng);

    auto end1 = Clock::now();

    // Local search
    auto [depth, nbgate] =
        local_cost_func(circuit, mapping, Dist);

    int depth1 = fitnessOG(circuit, mapping);

    bool change = true;
    int k = 0;

    auto start = Clock::now();

    while (change && k < 100) {
        ++k;

        std::tie(mapping, depth, nbgate, change) =
            GRASPlocal_search(
                circuit,
                mapping,
                depth,
                nbgate,
                Dist,
                local_cost_func
            );
    }

    auto end = Clock::now();

    int depth2 = fitnessOG(circuit, mapping);

    const double build_time =
        std::chrono::duration<double>(end1 - start1).count();

    const double local_time =
        std::chrono::duration<double>(end - start).count();

    std::cout << "Depth of build " << depth1
              << " " << build_time
              << " Depth of ls " << depth2
              << " " << local_time
              << " after " << k << " improvements\n";

    return {mapping, depth2};
}


std::pair<std::vector<int>, int>
GRASP(
    const ParsedCircuit& circuit,
    const DistanceMatrix& Dist,
    const CostFunction& cost_func,
    const LocalCostFunction& local_cost_func,
    const FinalCostFunction& fitnessOG,
    int num_iteration = 1,
    double alpha = 0.0,
    int workers = 1)
{
    if (num_iteration <= 0)
        throw std::invalid_argument(
            "num_iteration must be greater than zero"
        );

    const int max_workers = omp_get_num_procs();

    if (workers > max_workers) {
        std::cout << "Warning: workers = " << workers
                  << " > " << max_workers
                  << ". Workers reduced to " << max_workers
                  << '\n';

        workers = max_workers;
    }

    if (workers <= 0)
        workers = 1;

    std::vector<int> best_mapping;
    int best_depth = std::numeric_limits<int>::max();

    #pragma omp parallel num_threads(workers)
    {
        std::mt19937 rng(std::random_device{}());

        #pragma omp for schedule(dynamic)
        for (int task_id = 0; task_id < num_iteration; ++task_id) {

            auto [mapping, depth] = GRASPiteration(
                circuit,
                Dist,
                cost_func,
                local_cost_func,
                fitnessOG,
                alpha,
                task_id,
                rng
            );

            #pragma omp critical
            {
                if (depth < best_depth) {
                    best_mapping = std::move(mapping);
                    best_depth = depth;
                }
            }
        }
    }

    return {best_mapping, best_depth};
}


#endif