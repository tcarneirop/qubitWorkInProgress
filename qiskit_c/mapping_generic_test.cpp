
#include <iostream>
#include <cstdint>
#include <qiskit.h>

#include "qasm_parser.hpp"
#include "qiskit_circuit.hpp"
#include "mapping_utils.hpp"
#include "albatroz_target.hpp"
#include "routing_utils.hpp"
#include "circuit_metrics.hpp"

//make mapping-generic
//./mapping_generic_test.exe ../NEW_Bechmark/ising_model_16.qasm

/*
 * Verify that the mapping preserves the number of operations
 * and produces a DAG with the expected number of physical qubits.
 *
 * This test does not verify individual gate operands,
 * circuit equivalence, or hardware connectivity.
 * No routing is performed at this stage.
 */

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.qasm>\n";
        return 1;
    }

    /*
     * Step 1: Read the QASM file and build the logical DAG.
     */
    ParsedCircuit parsed = parse_qasm(argv[1]);

    QkCircuit* circuit = create_qiskit_circuit(parsed);
    QkDag* logical_dag = qk_circuit_to_dag(circuit);

    /*
     * Step 2: Define a physical mapping for Albatroz.
     *
     * Rotate the physical qubit indices by five positions.
     * This is a deterministic test, not an optimized mapping.
     */
    const uint32_t num_physical = 16;
    uint32_t mapping[16];

    for (uint32_t i = 0; i < num_physical; i++) {
        mapping[i] = (i + 5) % num_physical;
    }

    /*
     * Step 3: Apply the mapping to the complete logical DAG.
     */
    QkDag* physical_dag =
        apply_mapping(logical_dag, num_physical, mapping);

    if (physical_dag == nullptr) {
        std::cerr << "ERROR: Could not apply mapping\n";
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);
        return 1;
    }

    /*
     * Step 4: Verify that all circuit operations were preserved.
     */
    const size_t logical_ops = qk_dag_num_op_nodes(logical_dag);
    const size_t physical_ops = qk_dag_num_op_nodes(physical_dag);

    std::cout << "Logical qubits: "
              << qk_dag_num_qubits(logical_dag) << "\n";

    std::cout << "Physical qubits: "
              << qk_dag_num_qubits(physical_dag) << "\n";

    std::cout << "Original operations: " << logical_ops << "\n";
    std::cout << "Mapped operations: " << physical_ops << "\n";

    for (uint32_t i = 0; i < static_cast<uint32_t>(parsed.n); i++) {
        std::cout << "Logical " << i
                  << " -> Physical " << mapping[i] << "\n";
    }

    bool success =
        logical_ops == physical_ops &&
        qk_dag_num_qubits(physical_dag) == num_physical;

    std::cout << (success ? "SUCCESS" : "ERROR") << "\n";

        /*
    * Step 5: Evaluate the mapping using SABRE Routing.
    *
    * The mapping is fixed by us. SABRE is responsible only
    * for finding the SWAP operations required by the target.
    */
    if (!success) {
        qk_dag_free(physical_dag);
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);
        return 1;
    }

    /* Create the Albatroz hardware target. */
    QkTarget* target = create_albatroz_target();

    if (target == nullptr) {
        std::cerr << "ERROR: Could not create target\n";
        qk_dag_free(physical_dag);
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);
        return 1;
    }

    /* Store and validate the initial mapping in the transpiler state. */
    QkTranspilerStageState* state =
        create_mapping_state(
            logical_dag, target, mapping, parsed.n
        );

    if (state == nullptr) {
        std::cerr << "ERROR: Could not create mapping state\n";
        qk_target_free(target);
        qk_dag_free(physical_dag);
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);
        return 1;
    }

    /* Execute SABRE Routing without replacing our initial mapping. */
    if (!run_sabre_routing(physical_dag, target, state)) {
        qk_transpile_state_free(state);
        qk_target_free(target);
        qk_dag_free(physical_dag);
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);
        return 1;
    }

    /* Convert the routed DAG into a circuit for metric collection. */
    QkCircuit* routed_circuit = qk_dag_to_circuit(physical_dag);

    if (routed_circuit == nullptr) {
        std::cerr << "ERROR: Could not create routed circuit\n";
        qk_transpile_state_free(state);
        qk_target_free(target);
        qk_dag_free(physical_dag);
        qk_dag_free(logical_dag);
        qk_circuit_free(circuit);
        return 1;
    }

    /* Evaluate the circuit produced from our initial mapping. */
    std::cout << "\nSABRE Routing metrics:\n";

    print_circuit_stats(routed_circuit);

    std::cout << "Number of gates: "
            << circuit_num_gates(routed_circuit) << "\n";

    std::cout << "Depth: "
            << circuit_depth(routed_circuit) << "\n";

    qk_circuit_free(routed_circuit);
    qk_transpile_state_free(state);
    qk_target_free(target);



    /*
     * Step 6: Release all allocated Qiskit objects.
     */
    qk_dag_free(physical_dag);
    qk_dag_free(logical_dag);
    qk_circuit_free(circuit);

    return success ? 0 : 1;
}
