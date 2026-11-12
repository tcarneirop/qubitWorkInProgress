
#include <iostream>
#include <cstdint>
#include <qiskit.h>

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

    qk_circuit_free(circuit);

    return 0;
}
