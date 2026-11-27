from qiskit import QuantumCircuit
import numpy as np

def get_circuit_array(circuit):
    """
    Returns the circuit's gate sequence as a (num_gates, 2) numpy int32 array.
    For each gate, column 0 holds the first qubit index and column 1 holds
    the second qubit index (or -1 for single-qubit gates).

    Parameters:
        circuit (qiskit.circuit.quantumcircuit.QuantumCircuit): A quantum circuit.

    Return:
        numpy.ndarray : (num_gates, 2) array of qubit indices, C-contiguous.
    """
    qubit_to_idx = {q: i for i, q in enumerate(circuit.qubits)}
    data = circuit.data
    gates = np.empty((len(data), 2), dtype=np.int32)
    for i, inst in enumerate(data):
        qubits = inst.qubits
        gates[i, 0] = qubit_to_idx[qubits[0]]
        gates[i, 1] = qubit_to_idx[qubits[1]] if len(qubits) == 2 else -1
    return gates

def filter_circuit_qubits(circuit, keep_qubits):
    """
    Create a new circuit by keeping only selected qubits and
    dropping any gate involving other qubits.

    Parameters
    ----------
    circuit : QuantumCircuit
        Original circuit
    keep_qubits : list[int]
        Indices of qubits to keep (0-based)

    Returns
    -------
    QuantumCircuit
        Filtered circuit
    """
    keep_qubits = sorted(set(keep_qubits))
    new_n = len(keep_qubits)

    ### Map old qubit index -> new qubit index
    index_map = {old: new for new, old in enumerate(keep_qubits)}
    new_circ = QuantumCircuit(new_n, name=circuit.name)

    for inst in circuit.data:
        instruction = inst.operation
        qargs = inst.qubits
        cargs = inst.clbits
        ### Get original qubit indices
        old_indices = [circuit.qubits.index(q) for q in qargs]

        ### Skip instruction if it touches a dropped qubit
        if any(q not in index_map for q in old_indices):
            continue

        ### Remap qubits
        new_qargs = [new_circ.qubits[index_map[q]] for q in old_indices]

        ### Classical bits are ignored here (can be extended if needed)
        new_circ.append(instruction, new_qargs, [])
        
    return new_circ


''' generate GHZALL
qc=QuantumCircuit(nb_qubit)
    qc.h(0)
    for i in range(1,nb_qubit):
        qc.cx(0,i)
    qc.measure_all()
'''

def load_matrix_dat (input_file_path):

    with open(input_file_path, 'r') as f:
        lines = f.read().splitlines()
    
    # first line contains the size of the matrix
    size = int(lines[0].strip())
    
    # remaining lines contain the matrix (skipping the blank line at index 1)
    matrix_data = [list(map(int, line.split('\t'))) for line in lines[2:2+size]]
    
    # convert to numpy array
    matrix = np.array(matrix_data)

    return matrix, size


def display_input(circuit_name, device_name, n, N, with_sabre=True, population=None):

    print()
    if with_sabre:
        print("   >-------------------------------<")
        print("   >--- SABRE mapping + routing ---<")
        print("   >-------------------------------<")
    else:
        print("   >------------------------------<")
        print("   >--- Custom mapping + SABRE ---<")
        print("   >------------------------------<")
    print()

    print("---------------- input ----------------")
    print("quantum circuit:     ", circuit_name)
    print("quantum device:      ", device_name)
    print("logical qubits:      ", n)
    print("physical qubits:     ", N)
    if population is not None:
        print("population size:     ", population)
    print("---------------------------------------")


def display_output(num_gates, depth, runtime):

    print("---------------- output ---------------")
    print("number of gates:     ", num_gates)
    print("circuit depth:       ", depth)
    print("execution time:      ", runtime, "[s]")
    print("---------------------------------------")
    print()