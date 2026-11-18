
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
#include "routing_utils.hpp"
#include "circuit_metrics.hpp"
#include "mapping_utils.hpp"
#include "two_opt.hpp"
#include "three_opt.hpp"



int main(int argc, char* argv[]) {

    size_t num_trials = 5;
    int64_t sabre_seed = 42;
    uint32_t seed = 128;
    size_t heuristic = 2;

    if (argc < 3 || argc > 6) {
        std::cerr << "Usage: " << argv[0]
              << " <file.qasm> [heuristic - 2opt or 3opt] [mapping_seed] [num_trials] [sabre_seed]\n";
        return 1;
    }


    if (argc >= 3) {
        heuristic = static_cast<uint32_t>(std::stoul(argv[2]));
    }

    if (argc >= 4) {
        seed = static_cast<uint32_t>(std::stoul(argv[3]));
    }
    /*
    * SABRE Routing configuration.
    *
    * num_trials controls the number of internal routing attempts
    * performed for each candidate mapping.
    *
    * sabre_seed controls the stochastic SABRE search.
    */
   

    if (argc >= 5) {
        num_trials = std::stoull(argv[4]);
    }

    if (argc >= 6) {
        sabre_seed = std::stoll(argv[5]);
    }

    if (num_trials == 0) {
        std::cerr << "ERROR: num_trials must be greater than zero\n";
        return 1;
    }

    std::cout << "SABRE seed: " << sabre_seed << "\n";
    std::cout << "SABRE Routing trials: " << num_trials << "\n";




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
                num_logical, initial_result,sabre_seed, num_trials
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

        bool success = false;

        if (heuristic == 2) {
            success = two_opt(
                logical_dag, target, initial_mapping, num_logical,
                best_mapping, best_result, evaluations,
                total_routing_time, sabre_seed, num_trials
            );
        }
        else if (heuristic == 3) {
            success = three_opt(
                logical_dag, target, initial_mapping, num_logical,
                best_mapping, best_result, evaluations,
                total_routing_time, sabre_seed, num_trials
            );
        }
        if (!success) {
            std::cerr << "ERROR: Heuristic execution failed\n";
            qk_target_free(target);
            qk_dag_free(logical_dag);
            qk_circuit_free(circuit);
            return 1;
        }


        auto total_end = std::chrono::steady_clock::now();

        double total_time =
        std::chrono::duration<double>(total_end - total_start).count();


        /*
         * Step 6: Report the best result.
         */
        std::cout << "\n=== HEURISTIC RESULTS ===\n";

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
        std::cout << "Total heuristic evaluation time: "
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
