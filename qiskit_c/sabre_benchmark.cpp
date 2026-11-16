#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <qiskit.h>

#include "qasm_parser.hpp"
#include "qiskit_circuit.hpp"
#include "albatroz_target.hpp"
#include "layout_utils.hpp"
#include "circuit_metrics.hpp"
#include "benchmark_utils.hpp"

/* RAII wrappers guarantee cleanup, including when a run throws. */
using CircuitPtr = std::unique_ptr<QkCircuit, decltype(&qk_circuit_free)>;
using TargetPtr = std::unique_ptr<QkTarget, decltype(&qk_target_free)>;
using LayoutPtr = std::unique_ptr<QkTranspileLayout, decltype(&qk_transpile_layout_free)>;

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

int main(int argc, char* argv[]) {
    try {
        /* Parse the same circuit once, outside the timed region. */
        const BenchmarkOptions opt = parse_benchmark_args(argc, argv);
        const ParsedCircuit parsed = parse_qasm(opt.input);

        /* Use the same target for every run. */
        TargetPtr target(create_albatroz_target(), &qk_target_free);
        if (!target) throw std::runtime_error("Could not create Albatroz target");
        if (parsed.n > 16) throw std::runtime_error("Circuit has more than 16 logical qubits");

        std::cout << "Input: " << opt.input << "\nLogical qubits: " << parsed.n
                  << "\nOriginal gates: " << parsed.num_gates
                  << "\nPhysical qubits: 16"
                  << "\nRuns: " << opt.runs
                  << "\nSeed: " << opt.seed << " (step " << opt.seed_step << ')'
                  << "\nSABRE: iterations=" << opt.max_iterations
                  << " swap_trials=" << opt.num_swap_trials
                  << " random_trials=" << opt.num_random_trials << "\n";

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
            LayoutPtr layout(
                run_sabre_layout(circuit.get(), target.get(), seed,
                                 opt.max_iterations, opt.num_swap_trials,
                                 opt.num_random_trials),
                &qk_transpile_layout_free
            );
            const auto stop = std::chrono::steady_clock::now();
            if (!layout) throw std::runtime_error("SABRE Layout failed at run " + std::to_string(i + 1));

            const double elapsed = std::chrono::duration<double>(stop - start).count();

            /* Collect metrics OUTSIDE the timed region. */
            results.push_back({
                i + 1, seed, elapsed,
                qk_circuit_num_instructions(circuit.get()),
                circuit_num_gates(circuit.get()),
                count_swaps(circuit.get()),
                static_cast<std::size_t>(circuit_depth(circuit.get()))
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
