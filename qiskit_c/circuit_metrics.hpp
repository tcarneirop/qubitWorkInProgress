#ifndef CIRCUIT_METRICS
#define CIRCUIT_METRICS

#include <cstdint>
#include <cmath>
#include <qiskit.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iostream>

size_t circuit_num_gates(const QkCircuit *circuit) {

    QkOpCounts counts = qk_circuit_count_ops(circuit);

    size_t num_gates = 0;

    for (size_t i = 0; i < counts.len; i++) {

        const char *name = counts.data[i].name;

        // Exclude non-gate operations
        if (std::strcmp(name, "measure") == 0 ||
            std::strcmp(name, "reset") == 0 ||
            std::strcmp(name, "barrier") == 0) {
            continue;
        }

        num_gates += counts.data[i].count;
    }

    qk_opcounts_clear(&counts);

    return num_gates;
}
// Compute circuit depth
size_t circuit_depth(const QkCircuit *circuit) {

    const uint32_t num_qubits = qk_circuit_num_qubits(circuit);
    const size_t num_instructions = qk_circuit_num_instructions(circuit);

    std::vector<size_t> layers(num_qubits, 0);

    size_t depth = 0;

    for (size_t i = 0; i < num_instructions; i++) {

        QkCircuitInstruction inst = {};
        qk_circuit_get_instruction(circuit, i, &inst);

        // Ignore barriers
        if (std::strcmp(inst.name, "barrier") == 0) {
            qk_circuit_instruction_clear(&inst);
            continue;
        }

        // Find the latest occupied layer among involved qubits
        size_t max_layer = 0;

        for (uint32_t j = 0; j < inst.num_qubits; j++) {
            max_layer = std::max(
                max_layer,
                layers[inst.qubits[j]]
            );
        }

        // Place instruction in the next layer
        size_t new_layer = max_layer + 1;

        for (uint32_t j = 0; j < inst.num_qubits; j++) {
            layers[inst.qubits[j]] = new_layer;
        }

        depth = std::max(depth, new_layer);

        qk_circuit_instruction_clear(&inst);
    }

    return depth;
}

// Print circuit statistics
void print_circuit_stats(const QkCircuit *circuit) {

    std::cout << "Number of instructions: "
              << qk_circuit_num_instructions(circuit) << "\n";

    QkOpCounts counts = qk_circuit_count_ops(circuit);

    size_t swaps = 0;

    for (size_t i = 0; i < counts.len; i++) {

        std::cout << counts.data[i].name
                  << ": " << counts.data[i].count << "\n";

        if (std::strcmp(counts.data[i].name, "swap") == 0) {
            swaps = counts.data[i].count;
        }
    }

    std::cout << "SWAPs: " << swaps << "\n";

    qk_opcounts_clear(&counts);
}


#endif