
#include <iostream>
#include <cstdint>
#include <qiskit.h>
#include <chrono>

#include "albatroz_target.hpp"
#include "circuit_metrics.hpp"
#include "mapping_utils.hpp"



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
  

    // Create and validate mapping state
    QkTranspilerStageState* state =
        create_mapping_state(dag, target, mapping, num_logical);

    if (state == nullptr) {
        std::cerr << "ERROR: Could not create mapping state\n";
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }

    std::cout << "SUCCESS: Mapping stored in transpiler state\n";

    for (uint32_t i = 0; i < num_logical; i++) {
        std::cout << "Logical " << i
                << " -> Physical " << mapping[i] << "\n";
    }

  

    QkDag *physical_dag = apply_mapping(num_physical, mapping);

    // Print physical circuit
    QkCircuit *physical_circuit = qk_dag_to_circuit(physical_dag);

    char *physical_drawing = qk_circuit_draw(physical_circuit, nullptr);

    if (physical_drawing != nullptr) {
        std::cout << "Circuit after initial mapping:\n"
                << physical_drawing << "\n";
        qk_str_free(physical_drawing);
    }

    qk_circuit_free(physical_circuit);


    if (!run_sabre_routing(physical_dag, target, state)) {
        qk_dag_free(physical_dag);
        qk_transpile_state_free(state);
        qk_target_free(target);
        qk_dag_free(dag);
        qk_circuit_free(circuit);
        return 1;
    }


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


    //getting circuit metrics
    print_circuit_stats(routed_circuit);

    std::cout << "Number of gates: "
            << circuit_num_gates(routed_circuit) << "\n";

    std::cout << "Depth: "
            << circuit_depth(routed_circuit) << "\n";

    //freee
    qk_circuit_free(routed_circuit);
    qk_dag_free(physical_dag);

    qk_transpile_state_free(state);
    qk_target_free(target);
    qk_dag_free(dag);
    qk_circuit_free(circuit);

    return 0;
}
