
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

#include <Python.h>
#include <qiskit.h>
#include <memory>
#include <string>
#include <iomanip>


#include "qasm_parser.hpp"
#include "qiskit_circuit.hpp"
#include "albatroz_target.hpp"
#include "routing_utils.hpp"
#include "circuit_metrics.hpp"
#include "mapping_utils.hpp"
#include "two_opt.hpp"
#include "three_opt.hpp"
#include "topology_utils.hpp"
#include "scheduling_utils.hpp"

struct PythonGuard {
    ~PythonGuard() { Py_Finalize(); }
};

struct TargetDeleter {
    PyObject* owner = nullptr;

    void operator()(QkTarget* target) const {
        if (owner)
            Py_DECREF(owner);
        else
            qk_target_free(target);
    }
};

using TargetPtr = std::unique_ptr<QkTarget, TargetDeleter>;


int main(int argc, char* argv[]) {

    Py_Initialize();
    PythonGuard python_guard;

    if (qk_import() != 0) {
        PyErr_Print();
        return 1;
    }


    size_t num_trials = 5;
    uint32_t seed = std::random_device{}();
    int64_t sabre_seed = std::random_device{}();
    size_t heuristic = 2;
    uint8_t optimization_level = 0;
    bool translate = false;
    bool optimize = false;
    bool schedule = false;
    std::string scheduling = "asap";

    if (argc < 4 || argc > 12) {
        std::cerr << "Usage: " << argv[0]
            << " <file.qasm> <heuristic - 2opt or 3opt> <num_trials>"
            << " [optimization_level] [translate: 0|1] [optimize: 0|1]"
            << " [schedule: 0|1] [topology] [asap|alap]\n"
            << " [mapping_seed] [sabre_seed]\n";
        return 1;
    }

    heuristic = static_cast<size_t>(std::stoul(argv[2]));

    /* SABRE Routing configuration. */
    num_trials = std::stoull(argv[3]);

    if (num_trials == 0) {
        std::cerr << "ERROR: num_trials must be greater than zero\n";
        return 1;
    }

    if (argc >= 5) {
        const unsigned long level = std::stoul(argv[4]);

        if (level > 3) {
            std::cerr << "ERROR: optimization_level must be between 0 and 3\n";
            return 1;
        }

        optimization_level = static_cast<uint8_t>(level);
    }

    if (argc >= 6) {
        const unsigned long value = std::stoul(argv[5]);

        if (value > 1) {
            std::cerr << "ERROR: translate must be 0 or 1\n";
            return 1;
        }

        translate = (value == 1);
    }

    if (argc >= 7) {
        const unsigned long value = std::stoul(argv[6]);

        if (value > 1) {
            std::cerr << "ERROR: optimize must be 0 or 1\n";
            return 1;
        }

        optimize = (value == 1);
    }

    //HERE
    if (argc >= 8) {
        const unsigned long value = std::stoul(argv[7]);

        if (value > 1) {
            std::cerr << "ERROR: schedule must be 0 or 1\n";
            return 1;
        }

        schedule = (value == 1);
    }

    const std::string topology = argc >= 9 ? argv[8] : "albatroz";

    if (argc >= 10)
        scheduling = argv[9];

    if (argc >= 11)
        seed = std::stoul(argv[10]);

    if (argc >= 12)
        sabre_seed = std::stoll(argv[11]);


    if (scheduling != "asap" && scheduling != "alap") {
        std::cerr << "ERROR: scheduling must be asap or alap\n";
        return 1;
    }

    if (schedule && (!translate || !optimize)) {
        std::cerr << "ERROR: Scheduling requires Translation and Optimization\n";
        return 1;
    }
    std::cout << "Scheduling in the pipeline: "
          << (schedule ? "Yes" : "No") << "\n";

    if (schedule)
        std::cout << "Scheduling method: " << scheduling << "\n";


    //HERE
    std::cout << "Topology: " << topology << "\n";

    std::cout << "Heuristic: "
          << (heuristic == 2 ? "2-change" : "3-change") << "\n";
    std::cout << "Mapping seed: " << seed << "\n";
    std::cout << "SABRE seed: " << sabre_seed << "\n";
    std::cout << "SABRE Routing trials: " << num_trials << "\n";
    std::cout << "Optimization level: "
            << static_cast<unsigned int>(optimization_level) << "\n";
    std::cout << "Translating in the pipeline: "
            << (translate ? "Yes" : "No") << "\n";
    std::cout << "Optimizing in the pipeline: "
            << (optimize ? "Yes" : "No") << "\n";
    std::cout << "Scheduling in the pipeline: "
            << (schedule ? "Yes" : "No") << "\n";
    std::cout<<"############################################\n\n";

    try {
        /*
         * Step 1: Read the input quantum circuit.
         */
        ParsedCircuit parsed = parse_qasm(argv[1]);

        /* Load the hardware target before checking the circuit size. */
        TargetPtr target_owner(nullptr, TargetDeleter{});

        if (topology == "albatroz") {
            target_owner.reset(create_albatroz_target());
        } else {
            PythonTarget loaded = load_python_target(topology);
            target_owner = TargetPtr(loaded.target, TargetDeleter{loaded.owner});
        }

        QkTarget* target = target_owner.get();
        if (target == nullptr) {
            std::cerr << "ERROR: Could not create target\n";
            return 1;
        }

        PyObject* py_target = target_owner.get_deleter().owner;

        if (schedule) {
            if (!py_target) {
                std::cerr << "ERROR: Scheduling requires a Python target\n";
                return 1;
            }

            init_scheduling();
        }


        const uint32_t num_physical = qk_target_num_qubits(target);
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
         * Step 3: Generate a random initial mapping.
         *
         * The mapping is a permutation of all
         * physical qubits, including unused physical qubits.
         */

    
        std::mt19937 rng(seed);
        //std::mt19937 rng(sabre_seed);


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
            num_logical, initial_result, sabre_seed, num_trials,
            optimization_level, translate, optimize,
            schedule, py_target, scheduling.c_str()
        )) {
            std::cerr << "ERROR: Initial mapping evaluation failed\n";
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
         * The current search exchanges logical positions only.
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
                total_routing_time, sabre_seed, num_trials,
                optimization_level, translate, optimize,
                schedule, py_target, scheduling.c_str()
            );
        }
        else if (heuristic == 3) {
            success = three_opt(
                logical_dag, target, initial_mapping, num_logical,
                best_mapping, best_result, evaluations,
                total_routing_time, sabre_seed, num_trials,
                optimization_level, translate, optimize,
                schedule, py_target, scheduling.c_str()
            );
        }
        if (!success) {
            std::cerr << "ERROR: Heuristic execution failed\n";
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


        std::cout << "Initial mapping: ";
        print_mapping(initial_mapping);
        std::cout << "Initial depth: "
                  << initial_result.depth << "\n";
        std::cout << "Initial gates: "
          << initial_result.gates << "\n";
        std::cout << "Initial SWAPs: "
                  << initial_result.swaps << "\n";
        if (schedule) std::cout << std::fixed << std::setprecision(10)
          << "Initial makespan (s): "
          << initial_result.makespan_seconds
          << std::defaultfloat << std::setprecision(6) << '\n';

        std::cout << "\n====================\n";
        
        std::cout << "Best mapping: ";
        print_mapping(best_mapping);
        std::cout << "Best depth: "
                  << best_result.depth << "\n";
        std::cout << "Best gates: "
          << best_result.gates << "\n";
        std::cout << "Best SWAPs: "
                  << best_result.swaps << "\n";
        if (schedule) std::cout << std::fixed << std::setprecision(10)
          << "Best makespan (s): "
          << best_result.makespan_seconds
          << std::defaultfloat << std::setprecision(6) << '\n';

    
        std::cout << "\n====================\n";
        std::cout << "Neighbors evaluated: "
                  << evaluations << "\n";
        std::cout << "Total SABRE Routing time: "
                  << total_routing_time << " seconds\n";
        std::cout << "Total heuristic evaluation time: "
          << total_time << " seconds\n";


        /*
         * Step 7: Release shared Qiskit objects.
         */
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
