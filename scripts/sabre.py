from qiskit import QuantumCircuit
from qiskit.transpiler import Layout
from qiskit.transpiler.preset_passmanagers import generate_preset_pass_manager

import argparse
parser = argparse.ArgumentParser()

parser.add_argument("-c", "--circuit", help="Name and number of qubits of the circuit")

args = parser.parse_args()

############### Albatroz backend
from qiskit.transpiler import CouplingMap

coupling = CouplingMap([
    (0,1), (1,2), (2,3), (3,4), (4,5), (5,6), (6,7), (7,8), (8,9), (9,10), (10,11), (11,12), (12,13),
    (13,14), (14,15), (0,15), (1,14), (2,13), (3,12), (4,11), (5,10), (6,9)
])

from qiskit.transpiler import Target
from qiskit.circuit.library import IGate, XGate, CXGate, RZGate, SXGate, Reset
from qiskit.circuit import Measure

target = Target(num_qubits=16)

# 1-qubit gates
target.add_instruction(IGate(),   {(q,): None for q in range(target.num_qubits)})
target.add_instruction(XGate(),   {(q,): None for q in range(target.num_qubits)})
target.add_instruction(SXGate(),  {(q,): None for q in range(target.num_qubits)})
target.add_instruction(RZGate(0), {(q,): None for q in range(target.num_qubits)})
target.add_instruction(Reset(),   {(q,): None for q in range(target.num_qubits)})
target.add_instruction(Measure(), {(q,): None for q in range(target.num_qubits)})

# 2-qubit gates follow coupling graph
cx_edges = set(coupling.get_edges())
cx_edges |= {(b, a) for (a, b) in coupling.get_edges()}
target.add_instruction(CXGate(), {edge: None for edge in cx_edges})
   
from qiskit.providers import BackendV2
from qiskit.providers.options import Options
from qiskit.providers.exceptions import QiskitError

class CustomBackend(BackendV2):
    def __init__(self, target, coupling):
        super().__init__(name="custom_basis_backend")
        self._target = target
        self._coupling = coupling

    @property
    def target(self):
        return self._target

    @property
    def coupling_map(self):
        return self._coupling

    @classmethod
    def _default_options(cls):
        return Options(shots=1024)
   
    @property
    def max_circuits(self):
        return 1
   
    def run(self, run_input, **options):
        raise QiskitError(
            "This backend is for transpilation only and does not support execution."
        )
backend = CustomBackend(target, coupling)


##### Circuit
#folder = "./"
# file = "ising_model_16.qasm"
file = args.circuit
circuit_name = file.split('.')[0]  # name without extension
circuit=QuantumCircuit().from_qasm_file(file)
circuit.remove_final_measurements() ## Important, else bugs
qr=circuit.qregs[0]
nb_qubit = circuit.num_qubits
#print("Base circuit\n", circuit)
#print("Base circuit count ops", circuit.count_ops())
#print("Depth: ", circuit.depth())


def SABRE_post_trans_depth(qc,backend):
    pm = generate_preset_pass_manager(3,backend)
 
    for _ in range(len(pm.init)):
        pm.init.remove(0)
    
    # for _ in range(len(pm.translation)):
    #     pm.translation.remove(0)
    for _ in range(len(pm.optimization)):
        pm.optimization.remove(0)
    for _ in range(len(pm.scheduling)):
        pm.scheduling.remove(0)

    QC=pm.run(qc)
    #print(QC.count_ops())
    return QC.depth()

def sanity_depth_translated(qc,layout,backend):
    qr=qc.qregs[0]
    init_layout={qr[i]:layout[i] for i in range(len(layout))}
    init_layout=Layout(init_layout)

    pm = generate_preset_pass_manager(3,backend,initial_layout=init_layout)
   
    pm.layout.remove(1)
    pm.layout.remove(1)
   
    for _ in range(len(pm.init)):
        pm.init.remove(0)
    # for _ in range(len(pm.translation)):
    #     pm.translation.remove(0)
    for _ in range(len(pm.optimization)):
        pm.optimization.remove(0)
    for _ in range(len(pm.scheduling)):
        pm.scheduling.remove(0)

    QC=pm.run(qc)
    #print(QC.count_ops())
    return QC.depth()


print(sanity_depth_translated(circuit,[6, 5, 1, 12, 13, 11, 7, 15, 3, 14, 8, 2, 0, 9, 4, 10]
,backend))
print(SABRE_post_trans_depth(circuit,backend))