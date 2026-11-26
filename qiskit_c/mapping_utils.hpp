#ifndef MAPPING_UTILS_HPP
#define MAPPING_UTILS_HPP

#ifdef QISKIT_PYTHON_EXTENSION
#include "scheduling_utils.hpp"
#include <memory>
#endif

#include <cstdint>
#include <qiskit.h>
#include <vector>
#include <iostream>
#include "routing_utils.hpp"


/*
 * Print a complete physical-qubit mapping.
 */
void print_mapping(const std::vector<uint32_t>& mapping) {

    for (size_t i = 0; i < mapping.size(); i++) {
        std::cout << mapping[i]
                  << (i + 1 == mapping.size() ? "\n" : " ");
    }
}

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
 * Compare two mapping evaluations.
 *
 * Primary objective: minimize SWAPs.
 * Tie-breaking criterion: minimize circuit depth.
 */

bool is_better(
const MappingResult& candidate,
const MappingResult& best,
const bool schedule = false
) {
    if (schedule) {
        if (candidate.makespan_seconds == best.makespan_seconds)
            return candidate.swaps < best.swaps;
        return candidate.makespan_seconds < best.makespan_seconds;
    }

    if (candidate.depth == best.depth)
        return candidate.swaps < best.swaps;

    return candidate.depth < best.depth;
}



inline QkDag* apply_mapping(
    uint32_t num_physical,
    const uint32_t* mapping
) {
    QkDag *physical_dag = qk_dag_new();

    QkQuantumRegister *qr =
        qk_quantum_register_new(num_physical, "q");

    qk_dag_add_quantum_register(physical_dag, qr);
    qk_quantum_register_free(qr);

    uint32_t physical_h[] = {mapping[0]};
    uint32_t physical_cx[] = {mapping[0], mapping[2]};

    qk_dag_apply_gate(
        physical_dag, QkGate_H,
        physical_h, nullptr, false
    );

    qk_dag_apply_gate(
        physical_dag, QkGate_CX,
        physical_cx, nullptr, false
    );

    return physical_dag;
}


inline QkTranspilerStageState* create_mapping_state(
    const QkDag* dag,
    const QkTarget* target,
    const uint32_t* mapping,
    uint32_t num_logical
) {
    // Create layout
    QkTranspileLayout* layout =
        qk_transpile_layout_generate_from_mapping(
            dag, target, mapping
        );

    if (layout == nullptr)
        return nullptr;

    // Verify initial mapping
    std::vector<uint32_t> recovered(num_logical);

    bool ok = qk_transpile_layout_initial_layout(
        layout, true, recovered.data()
    );

    if (!ok) {
        qk_transpile_layout_free(layout);
        return nullptr;
    }

    for (uint32_t i = 0; i < num_logical; i++) {
        if (recovered[i] != mapping[i]) {
            qk_transpile_layout_free(layout);
            return nullptr;
        }
    }

    // Create transpiler state
    QkTranspilerStageState* state = nullptr;
    qk_transpile_state_new(&state);

    if (state == nullptr) {
        qk_transpile_layout_free(layout);
        return nullptr;
    }

    // Transfer ownership of layout to state
    qk_transpile_state_layout_set(state, layout);

    // Do not free layout: ownership was transferred
    layout = nullptr;

    // Verify mapping stored in state
    QkTranspileLayout* stored_layout =
        qk_transpile_state_layout(state);

    if (stored_layout == nullptr ||
        !qk_transpile_layout_initial_layout(
            stored_layout, true, recovered.data()
        )) {
        qk_transpile_state_free(state);
        return nullptr;
    }

    for (uint32_t i = 0; i < num_logical; i++) {
        if (recovered[i] != mapping[i]) {
            qk_transpile_state_free(state);
            return nullptr;
        }
    }

    return state;
}


/*
 * Apply a logical-to-physical mapping to an arbitrary quantum DAG.
 *
 * Parameters:
 *   logical_dag  - Original logical circuit represented as a DAG.
 *   num_physical - Number of physical qubits in the target.
 *   mapping      - mapping[i] is the physical qubit assigned
 *                  to logical qubit i.
 *
 * Returns:
 *   A new physical DAG containing all original operations,
 *   with qubit indices translated according to the mapping.
 *
 * Notes:
 *   - The original logical DAG is not modified.
 *   - Only circuits without classical bits are supported.
 *   - The caller must release the returned DAG.
 */
inline QkDag* apply_mapping(
    const QkDag* logical_dag,
    uint32_t num_physical,
    const uint32_t* mapping
) {
    if (logical_dag == nullptr || mapping == nullptr ||
        num_physical == 0) {
        return nullptr;
    }

    const uint32_t num_logical = qk_dag_num_qubits(logical_dag);

    if (num_logical > num_physical ||
        qk_dag_num_clbits(logical_dag) != 0) {
        return nullptr;
    }

    /*
     * Verify that the mapping is injective and every
     * physical qubit index belongs to the target.
     */
    std::vector<bool> used(num_physical, false);

    for (uint32_t i = 0; i < num_logical; i++) {
        if (mapping[i] >= num_physical || used[mapping[i]]) {
            return nullptr;
        }

        used[mapping[i]] = true;
    }

    /*
     * Create a new DAG with all physical qubits.
     */
    QkDag* physical_dag = qk_dag_new();

    if (physical_dag == nullptr) {
        return nullptr;
    }

    QkQuantumRegister* qr =
        qk_quantum_register_new(num_physical, "q");

    if (qr == nullptr) {
        qk_dag_free(physical_dag);
        return nullptr;
    }

    qk_dag_add_quantum_register(physical_dag, qr);
    qk_quantum_register_free(qr);

    /*
     * Copy the entire logical DAG into the physical DAG.
     *
     * The mapping array specifies which physical wire
     * receives each logical qubit.
     */
    QkExitCode status = qk_dag_compose(
        physical_dag,
        logical_dag,
        mapping,
        nullptr
    );

    if (status != QkExitCode_Success) {
        qk_dag_free(physical_dag);
        return nullptr;
    }

    return physical_dag;
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
    MappingResult& result,
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

    result.makespan_seconds = 0.0;

    if (schedule && (!translate || !optimize || !py_target))
        return false;

    #ifndef QISKIT_PYTHON_EXTENSION
    if (schedule)
        return false;
    #endif

    const uint32_t num_physical = qk_target_num_qubits(target);

     auto t0 = std::chrono::steady_clock::now();

    /* Create an independent physical DAG for this mapping. */
    QkDag* physical_dag =
        apply_mapping(logical_dag, num_physical, mapping.data());


    auto t1 = std::chrono::steady_clock::now();
   
    
    if (physical_dag == nullptr)
        return false;

    /* Create the transpiler state with our fixed mapping. */
    QkTranspilerStageState* state =
        create_mapping_state(
            logical_dag, target, mapping.data(), num_logical
        );


    auto t2 = std::chrono::steady_clock::now();

    static double total_mapping_time = 0.0;
    static double total_state_time = 0.0;
    static size_t evaluations = 0;

    total_mapping_time +=
        std::chrono::duration<double>(t1 - t0).count();

    total_state_time +=
        std::chrono::duration<double>(t2 - t1).count();

    ++evaluations;

    if (evaluations % 100 == 0) {
        std::cout << "[PROFILE] Evaluations: " << evaluations
                << " | apply_mapping: " << total_mapping_time
                << " s | create_mapping_state: " << total_state_time
                << " s\n";
    }

    if (state == nullptr) {
        qk_dag_free(physical_dag);
        return false;
    }

    /* Measure only the SABRE Routing execution time. */
    auto start = std::chrono::steady_clock::now();

    //std::cout << "Before routing: ";
    //for (auto q : mapping) std::cout << q << " ";
    //std::cout << '\n';

    bool success = run_sabre_routing(
        physical_dag, target, state,
        sabre_seed, num_trials, optimization_level
    );

    //std::cout << "After routing (input mapping): ";
    //for (auto q : mapping) std::cout << q << " ";
    //std::cout << '\n';

    auto end = std::chrono::steady_clock::now();

    result.time =
        std::chrono::duration<double>(end - start).count();

    if (!success) {
        qk_transpile_state_free(state);
        qk_dag_free(physical_dag);
        return false;
    }

    /* Count SWAPs before translation decomposes them. */
    QkCircuit* routing_circuit =
        qk_dag_to_circuit(physical_dag);

    if (routing_circuit == nullptr) {
        qk_transpile_state_free(state);
        qk_dag_free(physical_dag);
        return false;
    }

    const size_t routing_swaps = count_swaps(routing_circuit);

    QkOpCounts counts = qk_circuit_count_ops(routing_circuit);

    #ifdef VERBOSE
    for (size_t i = 0; i < counts.len; ++i) {
        std::cout << counts.data[i].name
                << ": " << counts.data[i].count << '\n';
    }
    #endif

   qk_opcounts_clear(&counts);

    /* Reuse the routed circuit when no additional stages are requested. */
    if (!translate && !optimize) {

        compute_circuit_metrics(routing_circuit, result);
        result.swaps = routing_swaps;

        qk_circuit_free(routing_circuit);
        qk_transpile_state_free(state);
        qk_dag_free(physical_dag);

        return true;
    }

    qk_circuit_free(routing_circuit);

    /* Translate the routed circuit into the target gate basis. */
    QkTranspileOptions options = qk_transpiler_default_options();
    options.optimization_level = optimization_level;
    options.seed = sabre_seed;

    char* error = nullptr;
    QkExitCode status;
    QkCircuit* routed_circuit;

    if (translate) {

        status = qk_transpile_stage_translation(
            physical_dag, target, &options, &error
        );

        if (status != QkExitCode_Success) {
            if (error != nullptr) {
                std::cerr << "Translation failed: " << error << '\n';
                qk_str_free(error);
            }

            qk_transpile_state_free(state);
            qk_dag_free(physical_dag);
            return false;
        }

    }


    if (optimize) {

        /* Optimize the translated circuit. */
        error = nullptr;

        status = qk_transpile_stage_optimization(
            physical_dag, target, &options, &error, state
        );

        if (status != QkExitCode_Success) {
            if (error != nullptr) {
                std::cerr << "Optimization failed: " << error << '\n';
                qk_str_free(error);
            }

            qk_transpile_state_free(state);
            qk_dag_free(physical_dag);
            return false;
        }


    }//optimize
    
    /* Convert the optimized DAG into a circuit. */
    routed_circuit = qk_dag_to_circuit(physical_dag);

    if (routed_circuit == nullptr) {
        qk_transpile_state_free(state);
        qk_dag_free(physical_dag);
        return false;
    }


    /* Collect depth and gate count. */
    compute_circuit_metrics(routed_circuit, result);

    /* Preserve the SWAP count obtained before translation. */
    result.swaps = routing_swaps;
    #ifdef QISKIT_PYTHON_EXTENSION
    if (schedule) {

        auto py_deleter = [](PyObject* obj) {
            Py_XDECREF(obj);
        };

        using PyObjectPtr =
            std::unique_ptr<PyObject, decltype(py_deleter)>;

        try {
            PyObjectPtr py_dag(
                qk_dag_to_python(physical_dag),
                py_deleter
            );

            physical_dag = nullptr;

            if (!py_dag) {
                PyErr_Print();
                throw std::runtime_error(
                    "Could not transfer DAG to Python"
                );
            }

            PyObjectPtr scheduled(
                run_scheduling(
                    py_dag.get(),
                    py_target,
                    scheduling
                ),
                py_deleter
            );

            result.makespan_seconds =
                estimate_makespan(scheduled.get(), py_target);

        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';

            qk_circuit_free(routed_circuit);
            qk_transpile_state_free(state);

            return false;
        }
    }
    #endif


    //here

    /* Release all objects created for this candidate. */
    qk_circuit_free(routed_circuit);
    qk_transpile_state_free(state);
    if (physical_dag) qk_dag_free(physical_dag);

    return true;
}


#endif