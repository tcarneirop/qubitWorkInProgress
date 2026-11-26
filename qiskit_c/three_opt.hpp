#ifndef THREE_OPT_HPP
#define THREE_OPT_HPP



#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>

#include "mapping_utils.hpp"

/*
 * Explore the complete non-identity 3-position neighborhood.
 *
 * For each triple i < j < k, generate all five
 * non-identity permutations of the selected values.
 *
 * Every candidate starts from the original mapping.
 * The search is non-recursive and explores one neighborhood.
 *
 * Note:
 *   Some candidates are equivalent to 2-opt moves and
 *   can be generated more than once across different triples.
 */
inline bool three_opt(
    const QkDag* logical_dag,
    const QkTarget* target,
    const std::vector<uint32_t>& initial_mapping,
    uint32_t num_logical,
    std::vector<uint32_t>& best_mapping,
    MappingResult& best_result,
    size_t& evaluations,
    double& total_routing_time,
    int64_t sabre_seed = 42,
    size_t num_trials = 5,
    uint8_t optimization_level = 0,
    bool translate = false,
    bool optimize = false,
    bool schedule = false,
    #ifdef QISKIT_PYTHON_EXTENSION
    PyObject* py_target = nullptr,
    #else
    void* py_target = nullptr,
    #endif
    const char* scheduling = "asap"

) {

    std::mt19937_64 rng(std::random_device{}());
    /* Generate all triples of physical-qubit positions. */
    for (uint32_t i = 0; i < num_logical; ++i) {
        for (uint32_t j = i + 1; j < num_logical; ++j) {
            for (uint32_t k = j + 1; k < num_logical; ++k) {

                std::array<int, 3> order = {0, 1, 2};
                std::array<uint32_t, 3> positions = {i, j, k};

                do {
                    /* Skip the unchanged mapping. */
                    if (order == std::array<int, 3>{0, 1, 2})
                        continue;

                    /* Always start from the original mapping. */
                    std::vector<uint32_t> candidate = initial_mapping;

                    candidate[i] = initial_mapping[positions[order[0]]];
                    candidate[j] = initial_mapping[positions[order[1]]];
                    candidate[k] = initial_mapping[positions[order[2]]];

                    /* Evaluate the candidate with fixed-layout SABRE Routing. */
                    MappingResult result;

                    if (!evaluate_mapping(
                        logical_dag, target, candidate,
                        num_logical, result, static_cast<int64_t>(rng()),
                        num_trials, optimization_level, translate, optimize,
                        schedule, py_target, scheduling
                    )) {
                        std::cerr << "ERROR: 3-opt candidate evaluation failed\n";
                        return false;
                    }

                    ++evaluations;
                    total_routing_time += result.time;
                    #ifdef VERBOSE
                    std::cout << "Candidate " << evaluations
                              << " (" << i << "," << j << "," << k << ")"
                              << " swaps=" << result.swaps
                              << " depth=" << result.depth
                              << " time=" << result.time << "\n";
                    #endif
                    /* Keep the best solution found so far. */
                    if (is_better(result, best_result,schedule)) {
                        best_mapping = candidate;
                        best_result = result;
                    }

                } while (std::next_permutation(
                    order.begin(), order.end()
                ));
            }
        }
    }

    return true;
}


#endif