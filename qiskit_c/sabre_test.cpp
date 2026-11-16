
#include <iostream>
#include <exception>
#include <vector>
#include <cstdint>
#include <qiskit.h>
#include <chrono>
#include "qasm_parser.hpp"
#include "qiskit_circuit.hpp"
#include "albatroz_target.hpp"
#include "layout_utils.hpp"
#include "circuit_metrics.hpp"

int main(int argc, char* argv[]) {

    /*
     * Step 1: Read the OpenQASM file and create
     * the original logical quantum circuit.
     */
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.qasm>\n";
        return 1;
    }

    try {
        ParsedCircuit parsed = parse_qasm(argv[1]);

        QkCircuit* circuit = create_qiskit_circuit(parsed);

        std::cout << "Logical qubits: " << parsed.n << "\n";
        std::cout << "Number of gates: " << parsed.num_gates << "\n";

        /*
         * Step 2: Create the Albatroz hardware target.
         *
         * The target defines the 16 physical qubits,
         * their connectivity and supported operations.
         */
        QkTarget* target = create_albatroz_target();

        if (target == nullptr) {
            std::cerr << "ERROR: Could not create Albatroz target\n";
            qk_circuit_free(circuit);
            return 1;
        }

        std::cout << "Physical qubits (Albatroz): 16\n";

        /*
         * Display the original logical circuit.
         */
        char* drawing = qk_circuit_draw(circuit, nullptr);

        //if (drawing != nullptr) {
        //    std::cout << "\nOriginal logical circuit:\n";
        //    std::cout << drawing << "\n";
        //    qk_str_free(drawing);
        //}

        /*
         * Step 3: Convert the logical circuit into a DAG.
         *
         * This representation will be useful for subsequent
         * experiments with individual transpiler stages.
         *
         * The standalone SABRE Layout pass below operates
         * directly on QkCircuit.
         */
        QkDag* dag = qk_circuit_to_dag(circuit);

        if (dag == nullptr) {
            std::cerr << "ERROR: Could not convert circuit to DAG\n";
            qk_target_free(target);
            qk_circuit_free(circuit);
            return 1;
        }

        std::cout << "SUCCESS: Logical circuit converted to DAG\n";

        /*
         * Step 4: Execute SABRE Layout + Routing.
         *
         * SABRE chooses an initial logical-to-physical
         * mapping and inserts SWAP gates when necessary.
         *
         * The input circuit is modified in place.
         */
        /*
        * Measure the execution time of SABRE Layout + Routing.
        *
        * The measurement excludes QASM parsing, target creation,
        * circuit construction and output formatting.
        */
        auto start = std::chrono::steady_clock::now();

        QkTranspileLayout* layout =
            run_sabre_layout(circuit, target, 42);

        auto end = std::chrono::steady_clock::now();

        double sabre_time =
            std::chrono::duration<double>(end - start).count();

        std::cout << "SABRE Layout + Routing time: "
                << sabre_time << " seconds\n";



        if (layout == nullptr) {
            std::cerr << "ERROR: SABRE Layout failed\n";
            qk_dag_free(dag);
            qk_target_free(target);
            qk_circuit_free(circuit);
            return 1;
        }

        /*
         * Step 5: Retrieve the initial mapping selected
         * automatically by SABRE.
         *
         * The output contains one physical qubit index
         * for each original logical qubit.
         */
        std::vector<uint32_t> initial_mapping(parsed.n);

        bool initial_ok = qk_transpile_layout_initial_layout(
            layout,
            true,
            initial_mapping.data()
        );

        if (initial_ok) {
            std::cout << "\nSABRE initial mapping:\n";

            for (int i = 0; i < parsed.n; i++) {
                std::cout << "Logical " << i
                          << " -> Physical "
                          << initial_mapping[i] << "\n";
            }
        } else {
            std::cerr << "ERROR: Could not retrieve initial mapping\n";
        }

        /*
         * Step 6: Display the physical circuit produced
         * by SABRE Layout + Routing.
         *
         * The circuit includes all physical qubits
         * and any SWAP gates inserted during routing.
         */
        std::cout << "\nCircuit after SABRE Layout + Routing:\n";

        char* routed_drawing = qk_circuit_draw(circuit, nullptr);

        //if (routed_drawing != nullptr) {
        //    std::cout << routed_drawing << "\n";
        //    qk_str_free(routed_drawing);
        //}

        /*
         * Step 7: Retrieve the final mapping after SABRE routing.
        */
        std::vector<uint32_t> final_mapping(parsed.n);

        qk_transpile_layout_final_layout(
            layout,
            true,
            final_mapping.data()
        );

        std::cout << "\nSABRE final mapping:\n";

        for (int i = 0; i < parsed.n; i++) {
            std::cout << "Logical " << i
                    << " -> Physical "
                    << final_mapping[i] << "\n";
        }

        /*
         * Step 8: Collect circuit metrics.
         *
         * Report the number of operations, SWAP gates,
         * total quantum gates and circuit depth.
         */
        std::cout << "\nSABRE circuit metrics:\n";

        print_circuit_stats(circuit);

        std::cout << "Number of gates: "
                  << circuit_num_gates(circuit) << "\n";

        std::cout << "Depth: "
                  << circuit_depth(circuit) << "\n";

        /*
         * Step 9: Release allocated Qiskit objects.
         *
         * The layout is returned independently by SABRE
         * and must be released separately.
         */
        qk_transpile_layout_free(layout);
        qk_dag_free(dag);
        qk_target_free(target);
        qk_circuit_free(circuit);

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
