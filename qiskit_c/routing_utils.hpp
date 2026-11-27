#ifndef ROUTING_UTILS_HPP
#define ROUTING_UTILS_HPP


#include <qiskit.h>
#include <iostream>


inline bool run_sabre_routing(
    QkDag* physical_dag,
    const QkTarget* target,
    QkTranspilerStageState* state,
    int64_t seed = 42,
    size_t num_trials = 5,
    uint8_t optimization_level = 0
) {
    QkTranspileOptions options = qk_transpiler_default_options();

    options.optimization_level = optimization_level;
    options.seed = seed;
    /* Number of internal SABRE Routing trials. */
    options.num_trials = num_trials;


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