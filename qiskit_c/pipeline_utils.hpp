#ifndef PIPELINE_UTILS_HPP
#define PIPELINE_UTILS_HPP

#include <qiskit.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "circuit_metrics.hpp"
#include "scheduling_utils.hpp"

using CircuitPtr = std::unique_ptr<QkCircuit, decltype(&qk_circuit_free)>;
using LayoutPtr = std::unique_ptr<QkTranspileLayout, decltype(&qk_transpile_layout_free)>;

inline void run_pipeline(
    QkCircuit* circuit,
    QkTarget* target,
    PyObject* py_target,
    LayoutPtr& layout,
    decltype(QkTranspileOptions::optimization_level) optimization_level,
    const char* scheduling,
    const std::chrono::steady_clock::time_point& start,
    CircuitPtr& translated,
    double& pipeline_seconds,
    double& makespan_seconds)
{
    using DagPtr = std::unique_ptr<QkDag, decltype(&qk_dag_free)>;


    /* Convert the routed circuit into a DAG. */
    DagPtr dag(
        qk_circuit_to_dag(circuit),
        &qk_dag_free
    );

    if (!dag)
        throw std::runtime_error(
            "Could not create routed DAG"
        );

    QkTranspileOptions options =
        qk_transpiler_default_options();

    options.optimization_level = optimization_level;

    char* error = nullptr;

    /* =========================================
     * 2. Translation
     * ========================================= */

    QkExitCode status = qk_transpile_stage_translation(
        dag.get(),
        target,
        &options,
        &error
    );

    if (status != QkExitCode_Success) {

        std::string message =
            error ? error : "Unknown error";

        if (error)
            qk_str_free(error);

        throw std::runtime_error(
            "Translation failed: " + message
        );
    }

    if (error) {
        qk_str_free(error);
        error = nullptr;
    }

    /* Create transpiler state. */
    QkTranspilerStageState* raw_state = nullptr;

    qk_transpile_state_new(&raw_state);

    if (!raw_state)
        throw std::runtime_error(
            "Could not create transpiler state"
        );

    std::unique_ptr<
        QkTranspilerStageState,
        decltype(&qk_transpile_state_free)
    > state(raw_state, &qk_transpile_state_free);

    /* Transfer ownership of the layout. */
    qk_transpile_state_layout_set(
        state.get(),
        layout.release()
    );

    /* =========================================
     * 3. Optimization
     * ========================================= */

    status = qk_transpile_stage_optimization(
        dag.get(),
        target,
        &options,
        &error,
        state.get()
    );

    if (status != QkExitCode_Success) {

        std::string message =
            error ? error : "Unknown error";

        if (error)
            qk_str_free(error);

        throw std::runtime_error(
            "Optimization failed: " + message
        );
    }

    if (error) {
        qk_str_free(error);
        error = nullptr;
    }

    /* Preserve the optimized circuit for metrics. */
    translated.reset(qk_dag_to_circuit(dag.get()));

    if (!translated)
        throw std::runtime_error(
            "Could not convert optimized DAG to circuit"
        );

    /* =========================================
     * 4. Scheduling
     * ========================================= */

    /* RAII for owned Python references. */
    auto py_deleter = [](PyObject* obj) {
        Py_XDECREF(obj);
    };

    using PyObjectPtr =
        std::unique_ptr<PyObject, decltype(py_deleter)>;

    /* Transfer DAG ownership to Python. */
    PyObjectPtr py_dag(
        qk_dag_to_python(dag.release()),
        py_deleter
    );

    if (!py_dag) {
        PyErr_Print();
        throw std::runtime_error(
            "Could not transfer DAG to Python"
        );
    }

    /* Execute the scheduling stage. */
    //receives the scheduling from the cli parameters
    PyObjectPtr scheduled(
        run_scheduling(
            py_dag.get(),
            py_target,
            scheduling
        ),
        py_deleter
    );

    /* Estimate QPU execution duration in seconds. */
    makespan_seconds = estimate_makespan(
        scheduled.get(),
        py_target
    );


    /* Stop timing after Scheduling. */
    const auto pipeline_stop =
        std::chrono::steady_clock::now();

    pipeline_seconds =
        std::chrono::duration<double>(
            pipeline_stop - start
        ).count();

    /* Report optimization and scheduling metrics. */
    std::cout << "Optimization completed: "
              << "gates="
              << circuit_num_gates(translated.get())
              << " depth="
              << circuit_depth(translated.get())
              << '\n';

    std::cout << "Scheduling completed: "
              << "makespan (s) = "
              << makespan_seconds
              << '\n';
}

#endif
