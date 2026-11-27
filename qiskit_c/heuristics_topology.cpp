
#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>

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
#include "heuristic_parameters.hpp"

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

    //getting the parameters from the command line
    const HeuristicParameters params = parameters_reading(argc, argv);
    print_parmeters(params);

    try {
        /*
         * Step 1: Read the input quantum circuit.
         */
        ParsedCircuit parsed = parse_qasm(argv[1]);

        /* Load the hardware target before checking the circuit size. */
        TargetPtr target_owner(nullptr, TargetDeleter{});

        if (params.topology == "albatroz") {
            target_owner.reset(create_albatroz_target());
        } else {
            PythonTarget loaded = load_python_target(params.topology);
            target_owner = TargetPtr(loaded.target, TargetDeleter{loaded.owner});
        }

        QkTarget* target = target_owner.get();
        if (target == nullptr) {
            std::cerr << "ERROR: Could not create target\n";
            return 1;
        }

        PyObject* py_target = target_owner.get_deleter().owner;

        if (params.schedule) {
            if (!py_target) {
                std::cerr << "ERROR: Scheduling requires a Python target\n";
                return 1;
            }

            init_scheduling();
        }


        const uint32_t num_physical = qk_target_num_qubits(target);
        const uint32_t num_logical = static_cast<uint32_t>(parsed.n);

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

    
        std::mt19937 rng(params.seed);
    
        std::vector<uint32_t> initial_mapping(num_physical);
        std::iota(
            initial_mapping.begin(), initial_mapping.end(), 0
        );

        std::shuffle(
            initial_mapping.begin(), initial_mapping.end(), rng
        );

    
        /*
         * Step 4: Evaluate the initial mapping.
         */
        auto total_start = std::chrono::steady_clock::now();

        MappingResult initial_result;

       if (!evaluate_mapping(
            logical_dag, target, initial_mapping,
            num_logical, initial_result, params.sabre_seed, params.num_trials,
            params.optimization_level, params.translate, params.optimize,
            params.schedule, py_target, params.scheduling.c_str()
        )) {
            std::cerr << "ERROR: Initial mapping evaluation failed\n";
            qk_dag_free(logical_dag);
            qk_circuit_free(circuit);
            return 1;
        }


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

       if (params.heuristic == 2) {
            success = two_opt(
                logical_dag, target, initial_mapping, num_logical,
                best_mapping, best_result, evaluations,
                total_routing_time, params.sabre_seed, params.num_trials,
                params.optimization_level, params.translate, params.optimize,
                params.schedule, py_target, params.scheduling.c_str()
            );
        }
        else if (params.heuristic == 3) {
            success = three_opt(
                logical_dag, target, initial_mapping, num_logical,
                best_mapping, best_result, evaluations,
                total_routing_time, params.sabre_seed, params.num_trials,
                params.optimization_level, params.translate, params.optimize,
                params.schedule, py_target, params.scheduling.c_str()
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

        print_heuristic_results(initial_mapping, initial_result, best_mapping, best_result, params,
             evaluations,total_routing_time,total_time );

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
