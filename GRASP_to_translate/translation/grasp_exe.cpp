
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <queue>
#include <limits>
#include <numeric>
#include <chrono>
#include <random>

#include "grasp.hpp"
#include "../../qiskit_c/qasm_parser.hpp"

DistanceMatrix create_jp_distance_matrix()
{
    constexpr int N = 16;

    const std::vector<std::pair<int, int>> edges = {
        {0,1}, {1,2}, {2,3}, {3,4},
        {4,5}, {5,6}, {6,7}, {7,8},
        {8,9}, {9,10}, {10,11}, {11,12},
        {12,13}, {13,14}, {14,15},
        {0,15}, {1,14}, {2,13}, {3,12},
        {4,11}, {5,10}, {6,9}
    };

    std::vector<std::vector<int>> adjacency(N);

    for (const auto& [a, b] : edges) {
        adjacency[a].push_back(b);
        adjacency[b].push_back(a);
    }

    DistanceMatrix Dist(
        N, std::vector<double>(
            N, std::numeric_limits<double>::infinity()
        )
    );

    // Shortest-path distances using BFS
    for (int source = 0; source < N; ++source) {
        std::queue<int> q;

        Dist[source][source] = 0;
        q.push(source);

        while (!q.empty()) {
            int u = q.front();
            q.pop();

            for (int v : adjacency[u]) {
                if (Dist[source][v] ==
                    std::numeric_limits<double>::infinity()) {

                    Dist[source][v] = Dist[source][u] + 1;
                    q.push(v);
                }
            }
        }
    }

    return Dist;
}

int main(int argc, char* argv[])
{
    std::string circuit_name;
    std::string backend = "JP";

    int iterations = 1;
    double alpha = 0.0;
    int workers = 1;

    // Help and usage
    if (argc < 2 ||
        std::string(argv[1]) == "-h" ||
        std::string(argv[1]) == "--help") {

        std::cout
            << "Usage: " << argv[0] << " [options]\n\n"
            << "Options:\n"
            << "  -c, --circuit <file.qasm>  Full path to QASM file\n"
            << "  -b, --backend <JP|OG>     Backend (default: JP)\n"
            << "  -i, --iteration <n>      GRASP iterations (default: 1)\n"
            << "  -a, --alpha <value>      GRASP alpha [0,1] (default: 0)\n"
            << "  -w, --workers <n>        OpenMP workers (default: 1)\n"
            << "  -h, --help               Show this help message\n";

        return argc < 2 ? 1 : 0;
    }

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-c" || arg == "--circuit") {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing QASM file");

            circuit_name = argv[++i];
        }
        else if (arg == "-b" || arg == "--backend") {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing backend");

            backend = argv[++i];
        }
        else if (arg == "-i" || arg == "--iteration") {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing iterations");

            iterations = std::stoi(argv[++i]);
        }
        else if (arg == "-a" || arg == "--alpha") {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing alpha");

            alpha = std::stod(argv[++i]);
        }
        else if (arg == "-w" || arg == "--workers") {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing workers");

            workers = std::stoi(argv[++i]);
        }
        else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    // Validate arguments
    if (circuit_name.empty())
        throw std::runtime_error("Missing QASM file (-c)");

    if (backend != "JP" && backend != "OG")
        throw std::runtime_error("Backend must be JP or OG");

    if (iterations <= 0)
        throw std::runtime_error("Iterations must be greater than zero");

    if (workers <= 0)
        throw std::runtime_error("Workers must be greater than zero");

    if (alpha < 0.0 || alpha > 1.0)
        std::cerr << "Warning: alpha outside [0,1]\n";

    ParsedCircuit circuit;
    DistanceMatrix Dist;

    // Circuit and backend configuration
    if (backend == "JP") {
        circuit = parse_qasm(circuit_name);
        Dist = create_jp_distance_matrix();

        if (circuit.n > static_cast<int>(Dist.size())) {
            throw std::runtime_error(
                "Circuit has more logical qubits than physical qubits"
            );
        }
    }
    else {
        throw std::runtime_error(
            "OG backend is not implemented yet in C++"
        );
    }

    std::cout << "Circuit: " << circuit_name << '\n';
    std::cout << "Logical qubits: " << circuit.n << '\n';
    std::cout << "Physical qubits: " << Dist.size() << '\n';
    std::cout << "Gates: " << circuit.num_gates << '\n';
    std::cout << "GRASP iterations: " << iterations << '\n';
    std::cout << "Alpha: " << alpha << '\n';
    std::cout << "Workers: " << workers << '\n';

    // Temporary final evaluation using C++ routing
    FinalCostFunction final_fitness =
        [&Dist](const ParsedCircuit& qc,
                const std::vector<int>& mapping) -> int {
            return static_cast<int>(
                fitness_cpp(qc, mapping, Dist)
            );
        };

    // Execute GRASP
    auto start = std::chrono::steady_clock::now();

    auto [best_mapping, best_depth] = GRASP(
        circuit,
        Dist,
        fitness_cpp,
        fitness_cpp_swap,
        final_fitness,
        iterations,
        alpha,
        workers
    );

    auto end = std::chrono::steady_clock::now();

    const double elapsed =
        std::chrono::duration<double>(end - start).count();

    std::cout << "\n=== GRASP RESULTS ===\n";

    std::cout << "Best mapping: ";
    for (int q : best_mapping)
        std::cout << q << ' ';

    std::cout << "\nBest depth: " << best_depth
              << "\nTotal time: " << elapsed
              << " seconds\n";

    return 0;
}
