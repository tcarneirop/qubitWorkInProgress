#include <iostream>
#include <cstdint>
#include <qiskit.h>

int main() {

    QkCircuit *circuit = qk_circuit_new(3, 0);

    if (circuit == nullptr) {
        std::cerr << "ERROR: Could not create circuit\n";
        return 1;
    }

    // Apply H to all three qubits
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t qubit[] = {i};

        QkExitCode status = qk_circuit_gate(
            circuit, QkGate_H, qubit, nullptr
        );

        if (status != QkExitCode_Success) {
            std::cerr << "ERROR: Could not add H gate\n";
            qk_circuit_free(circuit);
            return 1;
        }
    }

    // Apply CX: control = 0, target = 1
    uint32_t qubits[] = {0, 1};

    QkExitCode status = qk_circuit_gate(
        circuit, QkGate_CX, qubits, nullptr
    );

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Could not add CX gate\n";
        qk_circuit_free(circuit);
        return 1;
    }

    std::cout << "SUCCESS: H and CX gates added!\n";

    qk_circuit_free(circuit);
    return 0;
}

//q0: ──H──●──
//         │
//q1: ──H──X──
//q2: ──H─────