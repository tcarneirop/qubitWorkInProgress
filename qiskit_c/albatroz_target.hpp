#ifndef ALBATROZ_HPP
#define ALBATROZ_HPP


#include <cstdint>
#include <cmath>
#include <qiskit.h>



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


#endif
