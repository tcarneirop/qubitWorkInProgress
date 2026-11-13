
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <qiskit.h>


// Create the Albatroz target
QkTarget* create_albatroz_target() {

    QkTarget *target = qk_target_new(16);

    if (target == nullptr)
        return nullptr;

    // Albatroz coupling map
    uint32_t edges[][2] = {
        {0,1}, {1,2}, {2,3}, {3,4},
        {4,5}, {5,6}, {6,7}, {7,8},
        {8,9}, {9,10}, {10,11}, {11,12},
        {12,13}, {13,14}, {14,15},
        {0,15}, {1,14}, {2,13}, {3,12},
        {4,11}, {5,10}, {6,9}
    };

    const size_t num_edges = sizeof(edges) / sizeof(edges[0]);

    // Register CX connections in both directions
    QkTargetEntry *cx_entry = qk_target_entry_new(QkGate_CX);

    if (cx_entry == nullptr) {
        qk_target_free(target);
        return nullptr;
    }

    for (size_t i = 0; i < num_edges; i++) {

        uint32_t forward[] = {edges[i][0], edges[i][1]};
        uint32_t reverse[] = {edges[i][1], edges[i][0]};

        if (qk_target_entry_add_property(
                cx_entry, forward, 2, NAN, NAN) != QkExitCode_Success ||
            qk_target_entry_add_property(
                cx_entry, reverse, 2, NAN, NAN) != QkExitCode_Success) {

            qk_target_entry_free(cx_entry);
            qk_target_free(target);
            return nullptr;
        }
    }

    if (qk_target_add_instruction(target, cx_entry) != QkExitCode_Success) {
        qk_target_free(target);
        return nullptr;
    }

    // Register single-qubit gates
    QkGate gates[] = {
        QkGate_I,
        QkGate_X,
        QkGate_SX,
        QkGate_RZ
    };

    for (QkGate gate : gates) {

        QkTargetEntry *entry = qk_target_entry_new(gate);

        if (entry == nullptr) {
            qk_target_free(target);
            return nullptr;
        }

        for (uint32_t i = 0; i < 16; i++) {
            uint32_t qubit[] = {i};

            if (qk_target_entry_add_property(
                    entry, qubit, 1, NAN, NAN) != QkExitCode_Success) {
                qk_target_entry_free(entry);
                qk_target_free(target);
                return nullptr;
            }
        }

        if (qk_target_add_instruction(target, entry) != QkExitCode_Success) {
            qk_target_free(target);
            return nullptr;
        }
    }

    // Register Reset and Measure
    QkTargetEntry *operations[] = {
        qk_target_entry_new_reset(),
        qk_target_entry_new_measure()
    };

    if (operations[0] == nullptr || operations[1] == nullptr) {
        if (operations[0]) qk_target_entry_free(operations[0]);
        if (operations[1]) qk_target_entry_free(operations[1]);
        qk_target_free(target);
        return nullptr;
    }

    for (int op = 0; op < 2; op++) {

        QkTargetEntry *entry = operations[op];

        for (uint32_t i = 0; i < 16; i++) {
            uint32_t qubit[] = {i};

            if (qk_target_entry_add_property(
                    entry, qubit, 1, NAN, NAN) != QkExitCode_Success) {
                qk_target_entry_free(entry);
                if (op == 0) qk_target_entry_free(operations[1]);
                qk_target_free(target);
                return nullptr;
            }
        }

        if (qk_target_add_instruction(target, entry) != QkExitCode_Success) {
            if (op == 0) qk_target_entry_free(operations[1]);
            qk_target_free(target);
            return nullptr;
        }
    }

    return target;
}

int main() {

    const uint32_t num_logical = 3;
    const uint32_t num_physical = 16;

    // Create logical circuit
    QkCircuit *circuit = qk_circuit_new(num_logical, 0);

    if (circuit == nullptr) {
        std::cerr << "ERROR: Could not create circuit\n";
        return 1;
    }

    // H on logical qubit 0
    uint32_t q0[] = {0};

    QkExitCode status = qk_circuit_gate(
        circuit, QkGate_H, q0, nullptr
    );

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Could not add H\n";
        qk_circuit_free(circuit);
        return 1;
    }

    // CX between logical qubits 0 and 2
    uint32_t cx_qubits[] = {0, 2};

    status = qk_circuit_gate(
        circuit, QkGate_CX, cx_qubits, nullptr
    );

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Could not add CX\n";
        qk_circuit_free(circuit);
        return 1;
    }

    std::cout << "Original logical circuit:\n";

    char *drawing = qk_circuit_draw(circuit, nullptr);

    if (drawing != nullptr) {
        std::cout << drawing << "\n";
        qk_str_free(drawing);
    }

    // Logical-to-physical mapping
    uint32_t mapping[16] = {
        0, 5, 10,
        1, 2, 3, 4, 6, 7, 8, 9, 11, 12, 13, 14, 15
    };

    std::cout << "Physical qubits: " << num_physical << "\n";
    std::cout << "Logical qubits: " << num_logical << "\n";

    std::cout << "Initial mapping:\n";

    for (uint32_t i = 0; i < num_logical; i++) {
        std::cout << "Logical " << i
                  << " -> Physical " << mapping[i] << "\n";
    }

    // Create DAG representation
    QkDag *dag = qk_dag_new();

    QkQuantumRegister *qr =
        qk_quantum_register_new(num_logical, "q");

    qk_dag_add_quantum_register(dag, qr);
    qk_quantum_register_free(qr);

    // Add gates to DAG
    qk_dag_apply_gate(dag, QkGate_H, q0, nullptr, false);
    qk_dag_apply_gate(dag, QkGate_CX, cx_qubits, nullptr, false);

    // Create the complete Albatroz target
    QkTarget *target = create_albatroz_target();


    if (target == nullptr) {
        std::cerr << "ERROR: Could not create target\n";
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    // Generate layout from custom mapping
    QkTranspileLayout *layout =
        qk_transpile_layout_generate_from_mapping(
            dag,
            target,
            mapping
        );

    if (layout == nullptr) {
        std::cerr << "ERROR: Could not generate layout\n";
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    // Verify generated layout
    uint32_t initial_layout[3];

    bool ok = qk_transpile_layout_initial_layout(
        layout,
        true,
        initial_layout
    );

    if (!ok) {
        std::cerr << "ERROR: Initial layout not available\n";
        qk_transpile_layout_free(layout);
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    std::cout << "Generated QkTranspileLayout:\n";

    for (uint32_t i = 0; i < num_logical; i++) {
        std::cout << "Logical " << i
                  << " -> Physical " << initial_layout[i]
                  << "\n";
    }

    // Create transpiler stage state
    QkTranspilerStageState *state = nullptr;
    qk_transpile_state_new(&state);

    if (state == nullptr) {
        std::cerr << "ERROR: Could not create transpiler state\n";
        qk_transpile_layout_free(layout);
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    // Transfer layout ownership to transpiler state
    qk_transpile_state_layout_set(state, layout);
    layout = nullptr;

    // Retrieve layout from state
    QkTranspileLayout *stored_layout =
        qk_transpile_state_layout(state);

    if (stored_layout == nullptr) {
        std::cerr << "ERROR: Layout not found in state\n";
        qk_transpile_state_free(state);
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    uint32_t recovered_mapping[3];

    ok = qk_transpile_layout_initial_layout(
        stored_layout,
        true,
        recovered_mapping
    );

    if (!ok) {
        std::cerr << "ERROR: Could not recover mapping from state\n";
        qk_transpile_state_free(state);
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    std::cout << "Mapping stored in transpiler state:\n";

    for (uint32_t i = 0; i < num_logical; i++) {
        std::cout << "Logical " << i
                  << " -> Physical " << recovered_mapping[i]
                  << "\n";
    }

    // Free resources

        // Create physical DAG with 16 qubits
    QkDag *physical_dag = qk_dag_new();

    QkQuantumRegister *physical_qr =
        qk_quantum_register_new(num_physical, "q");

    qk_dag_add_quantum_register(physical_dag, physical_qr);
    qk_quantum_register_free(physical_qr);

    // Apply initial mapping
    uint32_t physical_h[] = {mapping[0]};
    uint32_t physical_cx[] = {mapping[0], mapping[2]};

    qk_dag_apply_gate(physical_dag, QkGate_H,
                    physical_h, nullptr, false);

    qk_dag_apply_gate(physical_dag, QkGate_CX,
                    physical_cx, nullptr, false);

    // Print physical circuit
    QkCircuit *physical_circuit = qk_dag_to_circuit(physical_dag);

    char *physical_drawing = qk_circuit_draw(physical_circuit, nullptr);

    if (physical_drawing != nullptr) {
        std::cout << "Circuit after initial mapping:\n"
                << physical_drawing << "\n";
        qk_str_free(physical_drawing);
    }

   qk_circuit_free(physical_circuit);

    // Run SABRE routing
    QkTranspileOptions options = qk_transpiler_default_options();
    options.optimization_level = 0;
    options.seed = 42;

    char *error = nullptr;

    status = qk_transpile_stage_routing(
        physical_dag,
        target,
        &options,
        state,
        &error
    );

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Routing failed\n";

        if (error != nullptr) {
            std::cerr << error << "\n";
            qk_str_free(error);
        }

        qk_dag_free(physical_dag);
        qk_transpile_state_free(state);
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    // Print routed circuit
    uint32_t initial_after[3];

    bool initial_ok = qk_transpile_layout_initial_layout(
        qk_transpile_state_layout(state),
        true,
        initial_after
    );

    if (initial_ok) {
        std::cout << "Initial mapping after routing:\n";

        for (uint32_t i = 0; i < num_logical; i++) {
            std::cout << "Logical " << i
                    << " -> Physical " << initial_after[i] << "\n";
        }
    }



    QkCircuit *routed_circuit = qk_dag_to_circuit(physical_dag);

    std::cout << "Circuit after routing:\n";

    char *routed_drawing = qk_circuit_draw(routed_circuit, nullptr);

    if (routed_drawing != nullptr) {
        std::cout << routed_drawing << "\n";
        qk_str_free(routed_drawing);
    }

    uint32_t final_mapping[3];

    qk_transpile_layout_final_layout(
        qk_transpile_state_layout(state),
        true,
        final_mapping
    );

    std::cout << "Final mapping:\n";

    for (uint32_t i = 0; i < num_logical; i++) {
        std::cout << "Logical " << i
                << " -> Physical " << final_mapping[i] << "\n";
    }

    
    qk_circuit_free(routed_circuit);
    qk_dag_free(physical_dag);

    qk_transpile_state_free(state);
    qk_target_free(target);
    qk_dag_free(dag);
    qk_circuit_free(circuit);

    return 0;
}
