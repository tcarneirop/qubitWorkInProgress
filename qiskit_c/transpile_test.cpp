
#include <iostream>
#include <cstdint>
#include <cmath>
#include <qiskit.h>
#include <cstring>
#include <vector>
#include <algorithm>

#include "albatroz_target.hpp"
#include "circuit_metrics.hpp"
// Create the Albatroz target


int main() {

    // Create logical circuit
    QkCircuit *circuit = qk_circuit_new(3, 0);

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

    // Create physical target
    QkTarget *target = create_albatroz_target();

    if (target == nullptr) {
        std::cerr << "ERROR: Could not create Albatroz target\n";
        qk_circuit_free(circuit);
        return 1;
    }

    std::cout << "Target: Albatroz\n";
    std::cout << "Physical qubits: "
              << qk_target_num_qubits(target) << "\n";

    // Configure transpilation
    QkTranspileOptions options = qk_transpiler_default_options();

    options.optimization_level = 3;
    options.seed = 42;

    // Run full transpilation
    QkTranspileResult result = {};
    char *error = nullptr;

    status = qk_transpile(
        circuit,
        target,
        &options,
        &result,
        &error
    );

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Transpilation failed\n";

        if (error != nullptr) {
            std::cerr << error << "\n";
            qk_str_free(error);
        }

        qk_circuit_free(circuit);
        qk_target_free(target);
        return 1;
    }

    std::cout << "SUCCESS: Transpilation completed!\n";

    std::cout << "Transpiled circuit:\n";

    drawing = qk_circuit_draw(result.circuit, nullptr);

    if (drawing != nullptr) {
        std::cout << drawing << "\n";
        qk_str_free(drawing);
    }
    print_circuit_stats(result.circuit);
    std::cout << "Number of gates: "
          << circuit_num_gates(result.circuit) << "\n";
    std::cout << "Depth: "
          << circuit_depth(result.circuit) << "\n";


    // Free resources
    qk_circuit_free(result.circuit);
    qk_transpile_layout_free(result.layout);
    qk_circuit_free(circuit);
    qk_target_free(target);

    return 0;
}
