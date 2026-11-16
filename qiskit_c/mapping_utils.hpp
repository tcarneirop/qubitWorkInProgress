#ifndef MAPPING_UTILS_HPP
#define MAPPING_UTILS_HPP

#include <cstdint>
#include <qiskit.h>
#include <vector>
#include <iostream>



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


#endif