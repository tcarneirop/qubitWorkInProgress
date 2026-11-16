#ifndef QISKIT_CIRCUIT_HPP
#define QISKIT_CIRCUIT_HPP


#pragma once

#include <qiskit.h>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "qasm_parser.hpp"

/*
 * Convert a parsed OpenQASM circuit into a Qiskit C API circuit.
 *
 * Input:
 *   pc - Circuit previously parsed by parse_qasm().
 *
 * Output:
 *   QkCircuit containing the original quantum gates,
 *   in the same order as the QASM file.
 *
 * Supported gates:
 *   H, X, CX, RZ and SX.
 *
 * Notes:
 *   - The caller is responsible for calling qk_circuit_free().
 *   - Unsupported gates produce an exception.
 *   - Gate parameters are obtained from pc.gate_params.
 */
inline QkCircuit* create_qiskit_circuit(const ParsedCircuit& pc) {

    /* Create a circuit with the number of logical qubits read from QASM. */
    QkCircuit *qc = qk_circuit_new(pc.n, 0);

    if (qc == nullptr)
        throw std::runtime_error("Could not create QkCircuit");

    /* Process each gate in the original circuit order. */
    for (int i = 0; i < pc.num_gates; i++) {

        const std::string& name = pc.gate_names[i];

        /* Retrieve the qubit indices associated with this gate. */
        uint32_t qubits[] = {
            static_cast<uint32_t>(pc.gates_flat[2*i]),
            static_cast<uint32_t>(pc.gates_flat[2*i+1])
        };

        QkGate gate;
        double parameter = pc.gate_params[i];
        const double *params = nullptr;

        /*
         * Translate the OpenQASM gate name into its corresponding
         * Qiskit C API gate identifier.
         */
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

        /* Append the gate and its parameters to the Qiskit circuit. */
        QkExitCode status = qk_circuit_gate(
            qc, gate, qubits, params
        );

        if (status != QkExitCode_Success) {
            qk_circuit_free(qc);
            throw std::runtime_error("Could not add gate: " + name);
        }
    }

    /* Return the circuit. Ownership belongs to the caller. */
    return qc;
}


#endif