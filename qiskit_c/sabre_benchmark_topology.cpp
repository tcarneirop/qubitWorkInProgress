#define QISKIT_PYTHON_EXTENSION

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <qiskit.h>
#include <random>
#include <cstdint>

#include "qasm_parser.hpp"
#include "qiskit_circuit.hpp"
#include "albatroz_target.hpp"
#include "layout_utils.hpp"
#include "circuit_metrics.hpp"
#include "benchmark_utils.hpp"
#include "topology_utils.hpp"
#include "pipeline_utils.hpp"

/* RAII wrappers guarantee cleanup, including when a run throws. */
struct TargetDeleter {
    PyObject *owner = nullptr;

    void operator()(QkTarget *target) const {
        if (owner)
            Py_DECREF(owner);
        else
            qk_target_free(target);
    }
};
using TargetPtr = std::unique_ptr<QkTarget, TargetDeleter>;

/* Count only actual SWAP instructions, not the number of CX used to
 * implement them after a later decomposition pass. */
static std::size_t count_swaps(const QkCircuit* circuit) {

    QkOpCounts counts = qk_circuit_count_ops(circuit);
    std::size_t swaps = 0;
    for (std::size_t i = 0; i < counts.len; ++i) {
        if (std::strcmp(counts.data[i].name, "swap") == 0)
            swaps = counts.data[i].count;
    }
    qk_opcounts_clear(&counts);
    return swaps;
}

struct PythonGuard {
    ~PythonGuard() {
        Py_Finalize();
    }
};

int main(int argc, char* argv[]) {

    Py_Initialize();
    PythonGuard python_guard;

    if (qk_import() != 0) {
        PyErr_Print();
        return 1;
    }

    try {

        /* Parse benchmark arguments and input circuit. */
        const BenchmarkOptions opt = parse_benchmark_args(argc, argv);
        const ParsedCircuit parsed = parse_qasm(opt.input);

        if (opt.pipeline)
            init_scheduling();

        /* Load hardware topology. */
        TargetPtr target(nullptr, TargetDeleter{});

        if (opt.topology == "albatroz") {
            target.reset(create_albatroz_target());
        } else {
            PythonTarget loaded = load_python_target(opt.topology);

            target = TargetPtr(
                loaded.target,
                TargetDeleter{loaded.owner}
            );
        }

        if (!target)
            throw std::runtime_error("Could not create target");

        const auto physical_qubits =
            qk_target_num_qubits(target.get());

        if (parsed.n > physical_qubits)
            throw std::runtime_error(
                "Circuit has more logical than physical qubits"
            );

        if (opt.pipeline && !target.get_deleter().owner)
            throw std::runtime_error(
                "Full pipeline with scheduling requires a Python target"
            );

        std::cout << "Input: " << opt.input
                  << "\nLogical qubits: " << parsed.n
                  << "\nOriginal gates: " << parsed.num_gates
                  << "\nPhysical qubits: " << physical_qubits
                  << "\nRuns: " << opt.runs
                  << "\nSeed: " << opt.seed
                  << " (step " << opt.seed_step << ')'
                  << "\nSABRE: iterations=" << opt.max_iterations
                  << " swap_trials=" << opt.num_swap_trials
                  << " random_trials=" << opt.num_random_trials << "\n"
                  << "Pipeline? " << opt.pipeline << "\n"
                  << "Optimization level: "
                  << (int)opt.optimization_level << "\n"
                  << "Scheduling: "
                  << opt.scheduling << "\n"
                  << "Topology: " << opt.topology << "\n";

        std::vector<BenchmarkResult> results;
        results.reserve(opt.runs);

        for (std::size_t i = 0; i < opt.runs; ++i) {

            const std::uint64_t seed =
                opt.seed + i * opt.seed_step;

            /* Create a clean circuit for every run. */
            CircuitPtr circuit(
                create_qiskit_circuit(parsed),
                &qk_circuit_free
            );

            if (!circuit)
                throw std::runtime_error("Could not create circuit");

            const auto start = std::chrono::steady_clock::now();

            /* =========================================
             * 1. SABRE Layout + Routing
             * ========================================= */

            LayoutPtr layout(
                run_sabre_layout(
                    circuit.get(),
                    target.get(),
                    seed,
                    opt.max_iterations,
                    opt.num_swap_trials,
                    opt.num_random_trials
                ),
                &qk_transpile_layout_free
            );

            if (!layout)
                throw std::runtime_error(
                    "SABRE Layout failed at run " +
                    std::to_string(i + 1)
                );

            const auto stop = std::chrono::steady_clock::now();

            const double elapsed =
                std::chrono::duration<double>(stop - start).count();

            const std::size_t routing_swaps =
                count_swaps(circuit.get());

            CircuitPtr translated(nullptr, &qk_circuit_free);

            double pipeline_seconds = 0.0;
            double makespan_seconds = 0.0;


            if (opt.pipeline) {
                run_pipeline(
                    circuit.get(),
                    target.get(),
                    target.get_deleter().owner,
                    layout,
                    opt.optimization_level,
                    opt.scheduling.c_str(),
                    start,
                    translated,
                    pipeline_seconds,
                    makespan_seconds
                );
            }

            /* Select the circuit used for structural metrics. */
            const QkCircuit* result_circuit =
                opt.pipeline ? translated.get() : circuit.get();

            results.push_back({
                i + 1,
                seed,
                elapsed,
                pipeline_seconds,
                makespan_seconds,
                qk_circuit_num_instructions(result_circuit),
                circuit_num_gates(result_circuit),
                routing_swaps,
                static_cast<std::size_t>(circuit_depth(result_circuit)),
            });
        }

        print_benchmark_results(results);
        write_benchmark_csv(opt.csv, results);

        if (!opt.csv.empty())
            std::cout << "CSV written: " << opt.csv << '\n';

        return 0;

    } catch (const std::exception& e) {

        std::cerr << "ERROR: " << e.what() << '\n';

        print_benchmark_usage(argv[0]);

        return 1;
    }
}