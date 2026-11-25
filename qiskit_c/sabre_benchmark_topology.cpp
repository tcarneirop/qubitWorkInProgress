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

/* RAII wrappers guarantee cleanup, including when a run throws. */
using CircuitPtr = std::unique_ptr<QkCircuit, decltype(&qk_circuit_free)>;
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
using LayoutPtr = std::unique_ptr<QkTranspileLayout, decltype(&qk_transpile_layout_free)>;
using DagPtr = std::unique_ptr<QkDag, decltype(&qk_dag_free)>;

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

    //@todo add     options.seed = static_cast<uint64_t>(rng()); instead of manual seed
    Py_Initialize();
    PythonGuard python_guard;

    if (qk_import() != 0) {
        PyErr_Print();
        Py_Finalize();
        return 1;
    }
    

    try {
        /* Parse the same circuit once, outside the timed region. */
        const BenchmarkOptions opt = parse_benchmark_args(argc, argv);

        const ParsedCircuit parsed = parse_qasm(opt.input);



        //let's get our topology
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

        const auto physical_qubits = qk_target_num_qubits(target.get());

        if (parsed.n > physical_qubits)
            throw std::runtime_error("Circuit has more logical than physical qubits");
        
        //END let's get our topology 



        std::cout << "Input: " << opt.input << "\nLogical qubits: " << parsed.n
                  << "\nOriginal gates: " << parsed.num_gates
                  << "\nPhysical qubits: " << physical_qubits
                  << "\nRuns: " << opt.runs
                  << "\nSeed: " << opt.seed << " (step " << opt.seed_step << ')'
                  << "\nSABRE: iterations=" << opt.max_iterations
                  << " swap_trials=" << opt.num_swap_trials
                  << " random_trials=" << opt.num_random_trials << "\n"
                  << "Pipeline? " << opt.pipeline <<"\n"
                  << "Optimization level: "<< (int)opt.optimization_level <<"\n"
                  << "Topology: "<< opt.topology <<"\n";
                  

        std::vector<BenchmarkResult> results;
        results.reserve(opt.runs);

        for (std::size_t i = 0; i < opt.runs; ++i) {



            const std::uint64_t seed = opt.seed + i * opt.seed_step;

            /* The SABRE pass modifies its QkCircuit input in place.
             * Rebuild a clean circuit for EVERY run; do not re-route
             * the output of a previous run. This is NOT timed. */
            CircuitPtr circuit(create_qiskit_circuit(parsed), &qk_circuit_free);
            if (!circuit) throw std::runtime_error("Could not create circuit");

            /* Time ONLY SABRE Layout + Routing. */
            const auto start = std::chrono::steady_clock::now();


            // 1. SABRE Layout + Routing
            LayoutPtr layout(
                run_sabre_layout(circuit.get(), target.get(), seed,
                                 opt.max_iterations, opt.num_swap_trials,
                                 opt.num_random_trials),
                &qk_transpile_layout_free
            );
            if (!layout)
                throw std::runtime_error(
                    "SABRE Layout failed at run " + std::to_string(i + 1)
                );
            /* Stop timing immediately after SABRE Layout + Routing. */
            const auto stop = std::chrono::steady_clock::now();

            const double elapsed =
                std::chrono::duration<double>(stop - start).count();

            const std::size_t routing_swaps = count_swaps(circuit.get());

            CircuitPtr translated(nullptr, &qk_circuit_free);


            //THIS IS TO EXECUTE THE FULL PIPELINE -- Translation, Optimization and Scheduling
            //THIS IS TO EXECUTE THE FULL PIPELINE -- Translation, Optimization and Scheduling
            //THIS IS TO EXECUTE THE FULL PIPELINE -- Translation, Optimization and Scheduling
           // const auto start_pipeline = std::chrono::steady_clock::now();
            
           double pipeline_seconds = 0.0f;

            if (opt.pipeline) {

                /* Convert the SABRE-routed circuit into a DAG. */
                DagPtr dag(
                    qk_circuit_to_dag(circuit.get()),
                    &qk_dag_free
                );

                if (!dag)
                    throw std::runtime_error("Could not create routed DAG");

                /* Configure the transpilation stages. */
                QkTranspileOptions options = qk_transpiler_default_options();
                options.optimization_level = opt.optimization_level;

                char* error = nullptr;

                /* Translation stage. */
                QkExitCode status = qk_transpile_stage_translation(
                    dag.get(),
                    target.get(),
                    &options,
                    &error
                );

                if (status != QkExitCode_Success) {
                    std::string message = error ? error : "Unknown error";

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

                /* Create a transpiler state for the optimization stage. */
                QkTranspilerStageState* raw_state = nullptr;
                qk_transpile_state_new(&raw_state);

                if (raw_state == nullptr)
                    throw std::runtime_error("Could not create transpiler state");

                std::unique_ptr<
                    QkTranspilerStageState,
                    decltype(&qk_transpile_state_free)
                > state(raw_state, &qk_transpile_state_free);

                /* Transfer ownership of the SABRE layout to the state. */
                qk_transpile_state_layout_set(
                    state.get(),
                    layout.release()
                );

                /* Optimization stage. */
                status = qk_transpile_stage_optimization(
                    dag.get(),
                    target.get(),
                    &options,
                    &error,
                    state.get()
                );

                if (status != QkExitCode_Success) {
                    std::string message = error ? error : "Unknown error";

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

                /* Scheduling stage: not implemented yet. */
                /* Stop timing after Optimization, before metric collection. */
                const auto pipeline_stop = std::chrono::steady_clock::now();
                pipeline_seconds = std::chrono::duration<double>(pipeline_stop - start).count();


                /* Convert the optimized DAG into a circuit. */
                translated.reset(qk_dag_to_circuit(dag.get()));

                if (!translated)
                    throw std::runtime_error(
                        "Could not convert optimized DAG to circuit"
                    );

                /* Collect metrics after Optimization. */
                std::cout << "Optimization completed: "
                        << "gates=" << circuit_num_gates(translated.get())
                        << " depth=" << circuit_depth(translated.get())
                        << '\n';
            }
         

            //END - THIS IS TO EXECUTE THE FULL PIPELINE -- Translation, Optimization and Scheduling
            //END - THIS IS TO EXECUTE THE FULL PIPELINE -- Translation, Optimization and Scheduling
            //END - THIS IS TO EXECUTE THE FULL PIPELINE -- Translation, Optimization and Scheduling

    

            /* Select the circuit from the last executed stage. */
            const QkCircuit* result_circuit =
                opt.pipeline ? translated.get() : circuit.get();

            /* Collect metrics after the selected pipeline. */
            results.push_back({
                i + 1,
                seed,
                elapsed,
                pipeline_seconds,
                qk_circuit_num_instructions(result_circuit),
                circuit_num_gates(result_circuit),
                routing_swaps,
                static_cast<std::size_t>(circuit_depth(result_circuit))
            });

        }


        print_benchmark_results(results);
        write_benchmark_csv(opt.csv, results);
        
        if (!opt.csv.empty()) std::cout << "CSV written: " << opt.csv << '\n';


        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        print_benchmark_usage(argv[0]);
        return 1;
    }
}
