#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

/* Common benchmark configuration. Other algorithms can reuse this structure
 * and ignore the SABRE-specific fields. */
struct BenchmarkOptions {
    
    std::string csv;
    std::string input;
    std::string topology = "albatroz";
    bool pipeline = false;
    std::size_t runs = 10;
    std::uint64_t seed = 42;
    double makespan_seconds = 0.0;
    std::uint64_t seed_step = 1;  // 0 repeats the same seed in every run
    std::size_t max_iterations = 4;
    std::size_t num_swap_trials = 20;
    std::size_t num_random_trials = 20;
    std::uint8_t optimization_level = 0;
    std::string scheduling = "asap";

};

/* One row per independent run. A later algorithm can reuse this type. */
struct BenchmarkResult {
    std::size_t run;
    std::uint64_t seed;
    double seconds;
    double pipeline_seconds;
    double makespan_seconds = 0.0;
    std::size_t instructions;
    std::size_t gates;
    std::size_t swaps;
    std::size_t depth;
};

inline void print_benchmark_usage(const char* exe) {
    std::cerr
        << "Usage: " << exe << " <file.qasm> [options]\n"
        << "  --runs N           Independent runs (default: 10)\n"
        << "  --seed N           First random seed (default: 42)\n"
        << "  --seed-step N      Seed increment per run (default: 1; 0=same seed)\n"
        << "  --iterations N     SABRE forward/backward iterations (default: 4)\n"
        << "  --swap-trials N    SABRE routing trials (default: 20)\n"
        << "  --random-trials N  SABRE random layout trials (default: 20)\n"
        << "  --csv PATH         Write per-run results to CSV\n"
        << "  --topology NAME    Hardware topology (default: albatroz)\n"
        << "  --scheduling METHOD  asap or alap (default: asap)\n"
        ;
}

/* Strict unsigned-integer parsing: reject negative values and trailing text. */
inline std::uint64_t parse_uint(const std::string& value, const std::string& flag) {
    if (value.empty() || value[0] == '-') {
        throw std::invalid_argument("Invalid value for " + flag + ": " + value);
    }
    std::size_t used = 0;
    unsigned long long n = std::stoull(value, &used, 10);
    if (used != value.size()) {
        throw std::invalid_argument("Invalid value for " + flag + ": " + value);
    }
    return static_cast<std::uint64_t>(n);
}

inline std::size_t checked_size(std::uint64_t value, const std::string& flag) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::out_of_range("Value too large for " + flag);
    }
    return static_cast<std::size_t>(value);
}

/* Parse CLI options without depending on an external argument parser. */
inline BenchmarkOptions parse_benchmark_args(int argc, char* argv[]) {


 

    if (argc < 2)
        throw std::invalid_argument("Missing QASM filename");

    BenchmarkOptions opt;
    bool seed_provided = false;

    opt.input = argv[1];

    

    if (opt.input.rfind("--", 0) == 0) {
        throw std::invalid_argument("Expected QASM filename first");
    }

    for (int i = 2; i < argc; ++i) {
        std::string flag = argv[i];

        if (flag == "--pipeline") {
            opt.pipeline = true;
            continue;
        }

        if (i + 1 >= argc)
            throw std::invalid_argument("Missing value for " + flag);

        std::string value = argv[++i];

        if (flag == "--runs")
            opt.runs = checked_size(parse_uint(value, flag), flag);

        else if (flag == "--seed") {
            opt.seed = parse_uint(value, flag);
            seed_provided = true;
        }
        else if (flag == "--optimization-level") {
            auto level = parse_uint(value, flag);

            if (level > 3)
                throw std::invalid_argument(
                    "Optimization level must be between 0 and 3"
                );

            opt.optimization_level = static_cast<std::uint8_t>(level);
        } 
        else if (flag == "--topology") {
            opt.topology = value;
        }
        else if (flag == "--csv") {
            opt.csv = value;
        }
        else if (flag == "--scheduling") {
            if (value != "asap" && value != "alap") throw std::invalid_argument("Scheduling must be asap or alap");
            opt.scheduling = value;
        }


        // Continue with your existing else-if options.
    }


    if (!opt.runs || !opt.max_iterations ||
        !opt.num_swap_trials || !opt.num_random_trials) {
        throw std::invalid_argument(
            "Runs, iterations and trial counts must be positive"
        );
    }

    /* Generate a seed when none was explicitly provided. */
    if (!seed_provided) {
        std::random_device rd;

        opt.seed =
            (static_cast<std::uint64_t>(rd()) << 32) |
             static_cast<std::uint64_t>(rd());
    }

    /* Check seed + (runs-1) * seed_step without unsigned overflow. */
    if (opt.seed_step && opt.runs > 1 &&
        (opt.runs - 1 >
         (std::numeric_limits<std::uint64_t>::max() - opt.seed) /
         opt.seed_step)) {
        throw std::out_of_range("Seed sequence would overflow uint64_t");
    }

    return opt;
}

inline void write_benchmark_rows(
    std::ostream& out,
    const std::vector<BenchmarkResult>& results)
{
    const auto old_precision = out.precision();

    out << "run,seed,seconds,pipeline_seconds,instructions,gates,swaps,depth,makespan_seconds\n";

    out << std::setprecision(17);

    for (const auto& r : results)
        out << r.run << ','
            << r.seed << ','
            << r.seconds << ','
            << r.pipeline_seconds << ','
            << r.instructions << ','
            << r.gates << ','
            << r.swaps << ','
            << r.depth << ','
            << r.makespan_seconds << '\n';

    out.precision(old_precision);
}

inline void print_benchmark_results(const std::vector<BenchmarkResult>& results) {


    std::cout << "\nRun results (time excludes parsing, circuit construction, metrics):\n";

    write_benchmark_rows(std::cout, results);


    /* Arithmetic mean and sample standard deviation (N-1). */
    auto report = [&](const char* name, auto extract) {
        double sum = 0;
        for (const auto& r : results) sum += extract(r);
        const double mean = sum / results.size();
        double squared = 0;
        for (const auto& r : results) {
            const double delta = extract(r) - mean;
            squared += delta * delta;
        }
        const double sd = results.size() > 1
            ? std::sqrt(squared / (results.size() - 1)) : 0.0;
        std::cout << name << ": mean=" << mean << " sample_stddev=" << sd << '\n';
    };
    std::cout << "\nSummary (" << results.size() << " runs):\n";
    report("seconds", [](const BenchmarkResult& r) { return r.seconds; });
    report("pipeline_seconds", [](const BenchmarkResult& r) {return r.pipeline_seconds;});
    report("swaps", [](const BenchmarkResult& r) { return double(r.swaps); });
    report("depth", [](const BenchmarkResult& r) { return double(r.depth); });
    report("makespan_seconds", [](const BenchmarkResult& r) {return r.makespan_seconds;});


    const auto best = std::min_element(results.begin(), results.end(),
        [](const BenchmarkResult& a, const BenchmarkResult& b) {
            if (a.depth != b.depth) return a.depth < b.depth;
            if (a.swaps != b.swaps) return a.swaps < b.swaps;
            return a.seconds < b.seconds;
        });

    const auto best_makespan = std::min_element(
            results.begin(), results.end(),
            [](const BenchmarkResult& a, const BenchmarkResult& b) {
        return a.makespan_seconds < b.makespan_seconds;
    });

    const auto print_best = [](const char* criterion, const BenchmarkResult& r) {
    std::cout << criterion
              << ": run=" << r.run
              << " seed=" << r.seed
              << " swaps=" << r.swaps
              << " depth=" << r.depth
              << " seconds=" << r.seconds
              << " makespan_seconds=" << r.makespan_seconds
              << '\n';
    };

    print_best("Best (min depth, then swaps, then time)", *best);

    if (best_makespan != results.end() &&
        best_makespan->makespan_seconds > 0.0)
        print_best("Best (min makespan)", *best_makespan);
        

}

inline void write_benchmark_csv(const std::string& path, const std::vector<BenchmarkResult>& results)
{
    if (path.empty()) return;

    std::ofstream out(path);

    if (!out)
        throw std::runtime_error("Cannot open CSV output: " + path);

    out << "run,seed,seconds,pipeline_seconds,instructions,gates,swaps,depth,makespan_seconds\n";

    out << std::setprecision(17);

    for (const auto& r : results)
        out << r.run << ','
            << r.seed << ','
            << r.seconds << ','
            << r.pipeline_seconds << ','
            << r.instructions << ','
            << r.gates << ','
            << r.swaps << ','
            << r.depth << ','
            << r.makespan_seconds << '\n';

    if (!out)
        throw std::runtime_error("Error writing CSV: " + path);
}
