#ifndef ROUTING_UTILS_HPP
#define ROUTING_UTILS_HPP


#include <qiskit.h>
#include <iostream>

inline bool run_sabre_routing(
    QkDag* physical_dag,
    const QkTarget* target,
    QkTranspilerStageState* state
) {
    QkTranspileOptions options = qk_transpiler_default_options();

    options.optimization_level = 0;
    options.seed = 42;

    char* error = nullptr;

    QkExitCode status = qk_transpile_stage_routing(
        physical_dag,
        target,
        &options,
        state,
        &error
    );

    if (status != QkExitCode_Success) {
        std::cerr << "ERROR: Routing failed\n";

        if (error != nullptr) {
            std::cerr << error << "\n";
            qk_str_free(error);
        }

        return false;
    }

    return true;
}

#endif