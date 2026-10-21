# -*- coding: utf-8 -*-
"""
Created on Wed Sep 30 15:28:59 2026

@author: Jérôme

Transforms a set of benchmarks into pre-translated benchmarks for SABRE-Routing calls using JP's C++ implementation
"""

from qiskit import QuantumCircuit
from qiskit.transpiler.preset_passmanagers import generate_preset_pass_manager

############### Albatroz backend
from qiskit.transpiler import CouplingMap

coupling = CouplingMap([
    (0,1), (1,2), (2,3), (3,4), (4,5), (5,6), (6,7), (7,8), (8,9), (9,10), (10,11), (11,12), (12,13),
    (13,14), (14,15), (0,15), (1,14), (2,13), (3,12), (4,11), (5,10), (6,9)
])

coupling = CouplingMap([(0,1), (1,2), (2,3), (3,4), (4,5), (5,6), (6,7), (7,8), (8,9), (9,10), (10,11), 
                        (11,12), (12,13), (13,14), (14,15), (0,15), (1,14), (2,13), (3,12), (4,11), (5,10), 
                        (6,9), (1,0), (2,1), (3,2), (4,3), (5,4), (6,5), (7,6), (8,7), (9,8), (10,9), (11,10), 
                        (12,11), (13,12), (14,13), (15,14), (15,0), (14,1), (13,2), (12,3), (11,4), (10,5), (9,6)])

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
    
# from qiskit.providers.fake_provider import Fake127QPulseV1, Fake5QV1, Fake20QV1, Fake27QPulseV1# New import for the backend
# from qiskit.providers import BackendV2Converter

# backend = Fake127QPulseV1()
# backend = BackendV2Converter(backend)

from qiskit.qasm2 import dumps

def init_trans(qc,backend):
    pm = generate_preset_pass_manager(3,backend)
    
    for _ in range(len(pm.layout)):
        pm.layout.remove(0)
    for _ in range(len(pm.routing)):
        pm.routing.remove(0)         
    # for _ in range(len(pm.translation)):
    #     pm.translation.remove(0)
    for _ in range(len(pm.optimization)):
        pm.optimization.remove(0)
    for _ in range(len(pm.scheduling)):
        pm.scheduling.remove(0)

    QC=pm.run(qc)
    return QC

from pathlib import Path

##### MODIFY THE FODLER HERE #####
folder = Path("./benchmark")

for file in folder.iterdir():
    if file.is_file():          # Skip subdirectories
        print(f"Processing {file.name}...")
        
        circuit=QuantumCircuit().from_qasm_file(file)
        circuit.remove_final_measurements() ## Important, else bugs
        
        with open(f"NEW_{str(file)}", "w") as f:
            f.write(dumps(init_trans(circuit,backend)))