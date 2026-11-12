
#include <iostream>
#include <cstdint>
#include <cmath>
#include <qiskit.h>
#include <cstring>
#include <vector>
#include <algorithm>

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
