
#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstdint>
#include <limits>
#include <cstring>
#include <exception>

#include <qiskit.h>

#include "qasm_parser.hpp"
#include "qiskit_circuit.hpp"
#include "albatroz_target.hpp"
#include "mapping_utils.hpp"
#include "routing_utils.hpp"
#include "circuit_metrics.hpp"

/*
 * Store the evaluation result for a candidate mapping.
 */
struct MappingResult {
    size_t swaps = 0;
    size_t depth = 0;
    double time = 0.0;
};

/*
 * Count the SWAP instructions inserted by SABRE Routing.
 */
size_t count_swaps(const QkCircuit* circuit) {

    QkOpCounts counts = qk_circuit_count_ops(circuit);
    size_t swaps = 0;

    for (size_t i = 0; i < counts.len; i++) {
        if (std::strcmp(counts.data[i].name, "swap") == 0) {
            swaps = counts.data[i].count;
            break;
        }
    }

    qk_opcounts_clear(&counts);
    return swaps;
}

/*
 * Evaluate a given logical-to-physical mapping.
 *
 * The original logical DAG is never modified.
 * A new physical DAG and transpiler state are created
 * for each candidate.
 *
 * The execution time measures only SABRE Routing.
 */
bool evaluate_mapping(
    const QkDag* logical_dag,
    const QkTarget* target,
    const std::vector<uint32_t>& mapping,
    uint32_t num_logical,
    MappingResult& result
) {
    const uint32_t num_physical = 16;

    /* Create an independent physical DAG for this mapping. */
    QkDag* physical_dag =
        apply_mapping(logical_dag, num_physical, mapping.data());

    if (physical_dag == nullptr)
        return false;

    /* Create the transpiler state with our fixed mapping. */
    QkTranspilerStageState* state =
        create_mapping_state(
            logical_dag, target, mapping.data(), num_logical
        );

    if (state == nullptr) {
        qk_dag_free(physical_dag);
        return false;
    }

    /* Measure only the SABRE Routing execution time. */
    auto start = std::chrono::steady_clock::now();

    bool success = run_sabre_routing(
        physical_dag, target, state
    );

    auto end = std::chrono::steady_clock::now();

    result.time =
        std::chrono::duration<double>(end - start).count();

    if (!success) {
        qk_transpile_state_free(state);
        qk_dag_free(physical_dag);
        return false;
    }

    /* Convert the routed DAG into a circuit for evaluation. */
    QkCircuit* routed_circuit =
        qk_dag_to_circuit(physical_dag);

    if (routed_circuit == nullptr) {
        qk_transpile_state_free(state);
        qk_dag_free(physical_dag);
        return false;
    }

    /* Collect the objective value and additional metrics. */
    result.swaps = count_swaps(routed_circuit);
    result.depth = circuit_depth(routed_circuit);

    /* Release all objects created for this candidate. */
    qk_circuit_free(routed_circuit);
    qk_transpile_state_free(state);
    qk_dag_free(physical_dag);

    return true;
}

/*
 * Compare two mapping evaluations.
 *
 * Primary objective: minimize SWAPs.
 * Tie-breaking criterion: minimize circuit depth.
 */
bool is_better(
    const MappingResult& candidate,
    const MappingResult& best
) {
    if (candidate.swaps != best.swaps)
        return candidate.swaps < best.swaps;

    return candidate.depth < best.depth;
}

/*
 * Print a complete physical-qubit mapping.
 */
void print_mapping(const std::vector<uint32_t>& mapping) {

    for (size_t i = 0; i < mapping.size(); i++) {
        std::cout << mapping[i]
                  << (i + 1 == mapping.size() ? "\n" : " ");
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <file.qasm> [seed]\n";
        return 1;
    }

    try {
        /*
         * Step 1: Read the input quantum circuit.
         */
        ParsedCircuit parsed = parse_qasm(argv[1]);

        const uint32_t num_physical = 16;
        const uint32_t num_logical =
            static_cast<uint32_t>(parsed.n);

        if (parsed.n <= 0 || num_logical > num_physical) {
            std::cerr << "ERROR: Invalid number of logical qubits\n";
            return 1;
        }

        QkCircuit* circuit = create_qiskit_circuit(parsed);
        QkDag* logical_dag = qk_circuit_to_dag(circuit);

        if (logical_dag == nullptr) {
            qk_circuit_free(circuit);
            return 1;
        }

        /*
         * Step 2: Create the Albatroz hardware target.
         */
        QkTarget* target = create_albatroz_target();

        if (target == nullptr) {
            qk_dag_free(logical_dag);
            qk_circuit_free(circuit);
            return 1;
        }

        /*
         * Step 3: Generate a random initial mapping.
         *
         * The mapping is a permutation of all 16
         * physical qubits, including unused physical qubits.
         */
        uint32_t seed = 128;

        if (argc >= 3) {
            seed = static_cast<uint32_t>(std::stoul(argv[2]));
        }

        std::mt19937 rng(seed);

        std::vector<uint32_t> initial_mapping(num_physical);
        std::iota(
            initial_mapping.begin(), initial_mapping.end(), 0
        );

        std::shuffle(
            initial_mapping.begin(), initial_mapping.end(), rng
        );

        std::cout << "Logical qubits: " << num_logical << "\n";
        std::cout << "Physical qubits: " << num_physical << "\n";
        std::cout << "Initial seed: " << seed << "\n";

        std::cout << "Initial mapping: ";
        print_mapping(initial_mapping);

        /*
         * Step 4: Evaluate the initial mapping.
         */
        auto total_start = std::chrono::steady_clock::now();

        MappingResult initial_result;

        if (!evaluate_mapping(
                logical_dag, target, initial_mapping,
                num_logical, initial_result
            )) {
            std::cerr << "ERROR: Initial mapping evaluation failed\n";
            qk_target_free(target);
            qk_dag_free(logical_dag);
            qk_circuit_free(circuit);
            return 1;
        }

        std::cout << "Initial SWAPs: "
                  << initial_result.swaps << "\n";

        std::cout << "Initial depth: "
                  << initial_result.depth << "\n";

        /*
         * Step 5: Generate the complete 2-opt neighborhood.
         *
         * Each neighbor exchanges two positions of the
         * original mapping.
         *
         * For 16 physical qubits:
         * 16 * 15 / 2 = 120 neighbors.
         */
        std::vector<uint32_t> best_mapping = initial_mapping;
        MappingResult best_result = initial_result;

        size_t evaluations = 0;
        double total_routing_time = initial_result.time;

        for (uint32_t i = 0; i < num_physical; i++) {
            for (uint32_t j = i + 1; j < num_physical; j++) {

                /* Generate one neighbor of the initial mapping. */
                std::vector<uint32_t> candidate = initial_mapping;
                std::swap(candidate[i], candidate[j]);

                /*
                 * Evaluate the candidate using SABRE Routing
                 * with its own independent physical DAG.
                 */
                MappingResult result;

                if (!evaluate_mapping(
                        logical_dag, target, candidate,
                        num_logical, result
                    )) {
                    std::cerr << "ERROR: Candidate evaluation failed\n";
                    qk_target_free(target);
                    qk_dag_free(logical_dag);
                    qk_circuit_free(circuit);
                    return 1;
                }

                evaluations++;
                total_routing_time += result.time;

                std::cout << "Candidate " << evaluations
                          << " (" << i << "," << j << ")"
                          << " swaps=" << result.swaps
                          << " depth=" << result.depth
                          << " time=" << result.time
                          << "\n";

                /*
                 * Retain the best candidate found so far.
                 */
                if (is_better(result, best_result)) {
                    best_mapping = candidate;
                    best_result = result;
                }
            }
        }//2-opt

        auto total_end = std::chrono::steady_clock::now();

        double total_time =
        std::chrono::duration<double>(total_end - total_start).count();


        /*
         * Step 6: Report the best result.
         */
        std::cout << "\n=== 2-OPT RESULTS ===\n";

        std::cout << "Neighbors evaluated: "
                  << evaluations << "\n";

        std::cout << "Initial SWAPs: "
                  << initial_result.swaps << "\n";

        std::cout << "Best SWAPs: "
                  << best_result.swaps << "\n";

        std::cout << "Initial depth: "
                  << initial_result.depth << "\n";

        std::cout << "Best depth: "
                  << best_result.depth << "\n";

        std::cout << "Initial mapping: ";
        print_mapping(initial_mapping);
        std::cout << "Best mapping: ";
        print_mapping(best_mapping);

        
        std::cout << "Total SABRE Routing time: "
                  << total_routing_time << " seconds\n";
        std::cout << "Total 2-opt evaluation time: "
          << total_time << " seconds\n";


        /*
         * Step 7: Release shared Qiskit objects.
         */
        qk_target_free(target);
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
