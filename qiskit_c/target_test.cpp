#include <iostream>
#include <cstdint>
#include <cmath>
#include <qiskit.h>

int main() {

    // Create an Albatroz target with 16 qubits
    QkTarget *target = qk_target_new(16);

    if (target == nullptr) {
        std::cerr << "ERROR: Could not create target\n";
        return 1;
    }

    std::cout << "Target: Albatroz\n";
    std::cout << "Number of qubits: "
              << qk_target_num_qubits(target) << "\n";

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

    // Create CX instruction
    QkTargetEntry *entry = qk_target_entry_new(QkGate_CX);

    for (size_t i = 0; i < num_edges; i++) {

        // Forward direction
        uint32_t forward[] = {edges[i][0], edges[i][1]};

        QkExitCode status = qk_target_entry_add_property(
            entry, forward, 2, NAN, NAN
        );

        if (status != QkExitCode_Success) {
            std::cerr << "ERROR: Could not add forward CX\n";
            qk_target_entry_free(entry);
            qk_target_free(target);
            return 1;
        }

        // Reverse direction
        uint32_t reverse[] = {edges[i][1], edges[i][0]};

        status = qk_target_entry_add_property(
            entry, reverse, 2, NAN, NAN
        );

        if (status != QkExitCode_Success) {
            std::cerr << "ERROR: Could not add reverse CX\n";
            qk_target_entry_free(entry);
            qk_target_free(target);
            return 1;
        }
    }

    // Register CX instruction
    QkExitCode status = qk_target_add_instruction(target, entry);

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Could not register CX instruction\n";
        qk_target_free(target);
        return 1;
    }

    std::cout << "SUCCESS: Albatroz coupling map created!\n";
    std::cout << "Physical connections: " << num_edges << "\n";
    std::cout << "Directed CX connections: " << 2 * num_edges << "\n";

    // Single-qubit gates supported by Albatroz
    QkGate gates[] = {
        QkGate_I,
        QkGate_X,
        QkGate_SX
    };

    for (QkGate gate : gates) {

        QkTargetEntry *gate_entry = qk_target_entry_new(gate);

        // Register gate on all 16 physical qubits
        for (uint32_t i = 0; i < 16; i++) {

            uint32_t qubit[] = {i};

            status = qk_target_entry_add_property(
                gate_entry, qubit, 1, NAN, NAN
            );

            if (status != QkExitCode_Success) {
                std::cerr << "ERROR: Could not add gate property\n";
                qk_target_entry_free(gate_entry);
                qk_target_free(target);
                return 1;
            }
        }

        // Register instruction in target
        status = qk_target_add_instruction(target, gate_entry);


        if (status != QkExitCode_Success) {
            std::cerr << "ERROR: Could not register gate\n";
            qk_target_free(target);
            return 1;
        }
    }

    std::cout << "SUCCESS: I, X and SX gates registered on all 16 qubits!\n";

        // Register parameterized RZ gate on all 16 qubits
    QkTargetEntry *rz_entry = qk_target_entry_new(QkGate_RZ);

    for (uint32_t i = 0; i < 16; i++) {

        uint32_t qubit[] = {i};

        status = qk_target_entry_add_property(
            rz_entry, qubit, 1, NAN, NAN
        );

        if (status != QkExitCode_Success) {
            std::cerr << "ERROR: Could not add RZ property\n";
            qk_target_entry_free(rz_entry);
            qk_target_free(target);
            return 1;
        }
    }

    // Register RZ instruction
    status = qk_target_add_instruction(target, rz_entry);

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Could not register RZ gate\n";
        qk_target_free(target);
        return 1;
    }

    std::cout << "SUCCESS: RZ gate registered on all 16 qubits!\n";

        // Register Reset and Measure on all 16 qubits
    QkTargetEntry *operations[] = {
        qk_target_entry_new_reset(),
        qk_target_entry_new_measure()
    };

    const char *names[] = {"Reset", "Measure"};

    for (int op = 0; op < 2; op++) {

        QkTargetEntry *op_entry = operations[op];

        for (uint32_t i = 0; i < 16; i++) {

            uint32_t qubit[] = {i};

            status = qk_target_entry_add_property(
                op_entry, qubit, 1, NAN, NAN
            );

            if (status != QkExitCode_Success) {
                std::cerr << "ERROR: Could not add "
                        << names[op] << " property\n";

                qk_target_entry_free(op_entry);
                if (op == 0) {
                    qk_target_entry_free(operations[1]);
                }
                qk_target_free(target);
                return 1;
            }
        }

        status = qk_target_add_instruction(target, op_entry);

        if (status != QkExitCode_Success) {
            std::cerr << "ERROR: Could not register "
                    << names[op] << "\n";

            if (op == 0) {
                qk_target_entry_free(operations[1]);
            }
            qk_target_free(target);
            return 1;
        }

        std::cout << "SUCCESS: " << names[op]
                << " registered on all 16 qubits!\n";
    }




    // Free resources
    qk_target_free(target);

    return 0;
}