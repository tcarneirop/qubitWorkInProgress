# -*- coding: utf-8 -*-
"""
Created on Wed Jul  20 09:07:20 2026

@author: Jérôme

TAKEN FROM extended
We are changing the local search. Here the local search looks at interaction
and tries to bring qubits that interact a lot to be close to each other
"""

import numpy as np
import time
from qiskit import QuantumCircuit,transpile,ClassicalRegister
from qiskit.transpiler import Layout
from qiskit.transpiler.preset_passmanagers import generate_preset_pass_manager
from GRAPS_func  import GRASP, fitness_cpp, fitnessOG

import argparse
parser = argparse.ArgumentParser()

##-------------------------------------------------------
##      Circuit selector
##-------------------------------------------------------
def circuit_selector(name,nb_qubit):
    if name=="ghzall":
        qc=QuantumCircuit(nb_qubit)
        qc.h(0)
        for i in range(1,nb_qubit):
            qc.cx(0,i)
        qc.measure_all()
          
        qr=qc.qregs[0]

        name="ghzall"
    else:
        filename=f"{name}_indep_qiskit_{nb_qubit}"
        qasmfile=f"../Benchmarks/MQTBench_all/{filename}.qasm"
        qc=QuantumCircuit().from_qasm_file(qasmfile)
        qr=qc.qregs[0]
    return qc,qr

parser.add_argument("-c", "--circuit", nargs=2, default=["ghzall", "80"], help="Name and number of qubits of the circuit")
parser.add_argument("-b", "--backend", default=["OG"], choices=["OG","JP"], help="Type of Backend")
parser.add_argument("-i", "--iteration", default=1, type=int, help="Number of GRAPS iteration")
parser.add_argument("-a", "--alpha", default=0, type=float, help="Alpha parameter in GRASP. Expected values are in [0,1]")
parser.add_argument("-w", "--workers", default=1, type=int, help="Number of parallel workers")
args = parser.parse_args()

##-------------------------------------------------------
##      Circuit & Backend selection
##-------------------------------------------------------
if args.backend == "OG":
    ### Backend
    from qiskit.providers.fake_provider import Fake127QPulseV1, Fake5QV1, Fake20QV1, Fake27QPulseV1# New import for the backend
    from qiskit.providers import BackendV2Converter
    
    backend = Fake127QPulseV1()
    backend = BackendV2Converter(backend)
    
    ##### Circuit (OG)
    circuit_name = args.circuit[0]
    nb_qubit = int(args.circuit[1])
    
    circuit,qr=circuit_selector(circuit_name, nb_qubit)
    circuit.remove_final_measurements() ## Important, else bugs
    
elif args.backend == "JP":
    ##### JP Backend
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
    
    # ### Falcon -- 27 qubits
    # from qiskit.providers.fake_provider import Fake127QPulseV1, Fake5QV1, Fake20QV1, Fake27QPulseV1# New import for the backend
    # from qiskit.providers import BackendV2Converter    
    # backend = Fake27QPulseV1()
    # backend = BackendV2Converter(backend)
    
    ##### Circuit (JP)
    folder = "./Bechmark/"
    # file = "ising_model_16.qasm"
    file = args.circuit[0]
    circuit_name = file.split('.')[0]  # name without extension
    circuit=QuantumCircuit().from_qasm_file(folder+file)
    circuit.remove_final_measurements() ## Important, else bugs
    qr=circuit.qregs[0]
    nb_qubit = circuit.num_qubits


##-------------------------------------------------------
##      Testing area 
##-------------------------------------------------------
numit = args.iteration
alpha = args.alpha
if not (0.0 <= alpha <=1.0): print(f"Warning: alpha = {alpha} is out of [0,1]. Expect uninteded results.")
workers = args.workers



start = time.time()
best_mapping, depthOG = GRASP(circuit, backend, fitness_cpp, num_iteration=numit, alpha=alpha, workers=workers)
end = time.time()
print(f"Best mapping for {circuit_name} {nb_qubit}:\n{best_mapping}\n Depth = {depthOG}")


##### Writing results in .txt
with open(f"bench_GRASP_matrix_{circuit_name}_{nb_qubit}_{numit}it_{alpha}a.txt","a") as fout:
    fout.write(f"{circuit_name} {nb_qubit} {depthOG} {end-start} {workers}\n")








