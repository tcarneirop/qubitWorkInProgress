#include <iostream>
#include "qasm_parser.hpp"

#include <qiskit.h>
#include <stdexcept>

QkCircuit* create_qiskit_circuit(const ParsedCircuit& pc) {

    QkCircuit *qc = qk_circuit_new(pc.n, 0);

    if (qc == nullptr)
        throw std::runtime_error("Could not create QkCircuit");

    for (int i = 0; i < pc.num_gates; i++) {

        const std::string& name = pc.gate_names[i];

        uint32_t qubits[] = {
            static_cast<uint32_t>(pc.gates_flat[2*i]),
            static_cast<uint32_t>(pc.gates_flat[2*i+1])
        };

        QkGate gate;
        double parameter = pc.gate_params[i];
        const double *params = nullptr;

        if (name == "h")
            gate = QkGate_H;
        else if (name == "x")
            gate = QkGate_X;
        else if (name == "cx")
            gate = QkGate_CX;
        else if (name == "sx")
            gate = QkGate_SX;
        else if (name == "rz") {
            gate = QkGate_RZ;
            params = &parameter;
        }
        else {
            qk_circuit_free(qc);
            throw std::runtime_error("Unsupported gate: " + name);
        }

        QkExitCode status = qk_circuit_gate(
            qc, gate, qubits, params
        );

        if (status != QkExitCode_Success) {
            qk_circuit_free(qc);
            throw std::runtime_error("Could not add gate: " + name);
        }
    }

    return qc;
}

int main(int argc, char** argv) {

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " circuit.qasm\n";
        return 1;
    }

    ParsedCircuit circuit = parse_qasm(argv[1]);

    std::cout << "Qubits: " << circuit.n << "\n";
    std::cout << "Gates: " << circuit.num_gates << "\n";

    for (int i = 0; i < circuit.num_gates; i++) {
    std::cout << circuit.gate_names[i] << " "
              << circuit.gates_flat[2*i] << " "
              << circuit.gates_flat[2*i+1]
              << " param=" << circuit.gate_params[i] << "\n";
    }

    QkCircuit *qc = create_qiskit_circuit(circuit);
    std::cout << "Number of gates: "
          << qk_circuit_num_instructions(qc) << "\n";
          

    char *drawing = qk_circuit_draw(qc, nullptr);

    if (drawing != nullptr) {
        std::cout << "\nQiskit circuit:\n" << drawing << "\n";
        qk_str_free(drawing);
    }

    qk_circuit_free(qc);

    return 0;
}

