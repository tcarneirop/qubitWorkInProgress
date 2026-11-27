#ifndef GRASP_GRAPH_HPP
#define GRASP_GRAPH_HPP


#include <vector>
#include <algorithm>
#include <numeric>
#include "../../qiskit_c/qasm_parser.hpp"

inline std::vector<std::vector<int>>
get_neighborhood_circ(const ParsedCircuit& circuit)
{
    std::vector<std::vector<int>> neighbors(circuit.n);

    for (int g = 0; g < circuit.num_gates; ++g) {
        const int q0 = circuit.gates_flat[2 * g];
        const int q1 = circuit.gates_flat[2 * g + 1];

        if (q1 == -1)
            continue;

        auto& n0 = neighbors[q0];
        auto& n1 = neighbors[q1];

        if (std::find(n0.begin(), n0.end(), q1) == n0.end())
            n0.push_back(q1);

        if (std::find(n1.begin(), n1.end(), q0) == n1.end())
            n1.push_back(q0);
    }

    return neighbors;
}


inline std::vector<int>
get_init_qubit_list(const ParsedCircuit& circuit)
{
    std::vector<int> prio(circuit.n, 0);

    for (int g = 0; g < circuit.num_gates; ++g) {
        const int q0 = circuit.gates_flat[2 * g];
        const int q1 = circuit.gates_flat[2 * g + 1];

        if (q1 == -1)
            continue;

        ++prio[q0];
        ++prio[q1];
    }

    std::vector<int> order(circuit.n);
    std::iota(order.begin(), order.end(), 0);

    std::stable_sort(order.begin(), order.end(),
        [&prio](int a, int b) {
            return prio[a] > prio[b];
        });

    return order;
}


inline ParsedCircuit
filter_circuit_qubits(
    const ParsedCircuit& circuit,
    std::vector<int> keep_qubits)
{
    std::sort(keep_qubits.begin(), keep_qubits.end());

    keep_qubits.erase(
        std::unique(keep_qubits.begin(), keep_qubits.end()),
        keep_qubits.end()
    );

    ParsedCircuit filtered;
    filtered.n = static_cast<int>(keep_qubits.size());

    // Original qubit index -> new qubit index
    std::vector<int> index_map(circuit.n, -1);

    for (int i = 0; i < filtered.n; ++i)
        index_map[keep_qubits[i]] = i;

    for (int g = 0; g < circuit.num_gates; ++g) {
        const int q0 = circuit.gates_flat[2 * g];
        const int q1 = circuit.gates_flat[2 * g + 1];

        // Skip gates involving discarded qubits
        if (index_map[q0] == -1)
            continue;

        if (q1 != -1 && index_map[q1] == -1)
            continue;

        // Preserve the original operation
        filtered.gate_names.push_back(circuit.gate_names[g]);
        filtered.gate_params.push_back(circuit.gate_params[g]);

        // Remap qubit indices
        filtered.gates_flat.push_back(index_map[q0]);
        filtered.gates_flat.push_back(
            q1 == -1 ? -1 : index_map[q1]
        );

        ++filtered.num_gates;
    }

    return filtered;
}




#endif