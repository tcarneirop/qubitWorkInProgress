

#include <qiskit.h>
#include <stdexcept>
#include <iostream>
#include "qasm_parser.hpp"
#include "qiskit_circuit.hpp"



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

