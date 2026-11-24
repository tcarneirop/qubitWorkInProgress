# -*- coding: utf-8 -*-
"""
Created on Wed Jul  20 09:04:23 2026

@author: Jérôme

TAKEN FROM extended
We are changing the local search. Here the local search looks at interaction
and tries to bring qubits that interact a lot to be close to each other
"""

from itertools import product
import numpy as np
from qiskit import QuantumCircuit
from qiskit.transpiler import Layout
from qiskit.transpiler.preset_passmanagers import generate_preset_pass_manager
from qiskit.converters import circuit_to_dag
# from SABRE_copy_func import SABRE_routing,get_circuit_dic
import routing    ### C++ routing by JP
from utils import get_circuit_array  ### Utils function by JP
import time

from concurrent.futures import ProcessPoolExecutor, as_completed
import os


def GRASP(circuit, backend, cost_func, num_iteration=1, alpha=0, workers=os.cpu_count()):
    # Dist = backend.coupling_map.distance_matrix
    results = {}
    if workers > os.cpu_count():
        print(f"Warning: workers = {workers} > {os.cpu_count()} = os.cpu_count(). workers reduced to {os.cpu_count()}")
        workers = os.cpu_count()

    # Run all iterations in parallel
    with ProcessPoolExecutor(max_workers=workers) as executor:
        # Submit all jobs
        futures = [
            executor.submit(GRASPiteration, circuit, backend, cost_func, alpha, task_id)
            for task_id in range(num_iteration)
        ]

        best_mapping = None
        best_depth = float("inf")

        # Collect results as they finish
        for future in as_completed(futures):
            mapping, depth = future.result()
            if depth < best_depth:
                best_mapping = mapping.copy()
                best_depth = depth
    return best_mapping, best_depth


def GRASPiteration(circuit, backend, cost_func, alpha, task_id):
    Dist = backend.coupling_map.distance_matrix
    ### Fixes a unique seed for each iteration
    seed = os.getpid()
    np.random.seed(seed)
    ### Build phase
    start1 = time.time()
    mapping = GRASPbuild(circuit, Dist, cost_func, alpha, task_id)
    end1 = time.time()
    ### Local search 
    depth, nbgate = fitness_cpp_swap(circuit, mapping, Dist)
    #depth1 = fitnessOG(circuit, mapping, backend)
    change = True
    k = 0    
    start = time.time()
    while change and k<100:
        # print(k)
        k+=1
        mapping, depth, nbgate, change = GRASPlocal_search(circuit, mapping, depth, nbgate, Dist, fitness_cpp_swap)
    end = time.time()
    #depth2 = fitnessOG(circuit, mapping, backend)
    print(f"Depth of build {depth} {end1-start1} Depth of ls {depth} {end-start} after {k} improvements")
    return mapping, depth


def GRASPbuild(circuit, Dist, cost_func, alpha, task_id):
    nb_qubit_circ = circuit.num_qubits
    nb_qubit_back = len(Dist)
    
    Neighborhood_circ = get_neighborhood_circ(circuit)
    Neighborhood_back = {i:{j for j in range(nb_qubit_back) if Dist[i,j]==1} for i in range(nb_qubit_back)}
     
    ### Initialise the mapping with a priority to more connected to central qubit  
    keep_qubits = [get_init_qubit_list(circuit)[task_id%nb_qubit_circ]]
    if nb_qubit_back == 127:
        mapping = [int(np.random.choice([62,63,64]))]
    else:
        mapping = [int(np.random.choice([3,4,11,12]))]
    for i in range(nb_qubit_circ-1):
        neighbor_circ = {x for k in keep_qubits for x in Neighborhood_circ[k]}.difference(set(keep_qubits))
        neighbor_back = {x for k in mapping for x in Neighborhood_back[k]}.difference(set(mapping))
        ### Compute depth for each pair of neighbor_circ/back to assign probabilities
        pairs = {}
        for q,p in product(neighbor_circ,neighbor_back):
            ### Update keep_qubit and mapping for testing
            for j in range(len(keep_qubits)):
                if keep_qubits[j]>q:
                    keep_qubits_testing = keep_qubits[:j] + [q] + keep_qubits[j:]
                    mapping_testing = mapping[:j] + [np.random.choice(list(neighbor_back))] + mapping[j:]
                    break
            else: ### This case is reached only if new_qubit is larger than any qubits in keep_qubits
                keep_qubits_testing = keep_qubits + [q]
                mapping_testing = mapping + [p]
            ### Derive partial circuit
            partial_circ = filter_circuit_qubits(circuit, keep_qubits_testing)
            # print(partial_circ)
            ### Store depth of partial circuit for this pair.
            pairs[(q,p)] = cost_func(partial_circ, mapping_testing, Dist)
            
        ### Greedy randomised choice
        # print(sorted(pairs.values()))
        minval, maxval = min(pairs.values()), max(pairs.values())
        threshold = minval + alpha*(maxval - minval)
        best_pairs = {key: val for key, val in pairs.items() if val <= threshold}
        best_pairs = list(best_pairs.items())
        ### New logical qubit q is placed on physical qubit p
        q,p = best_pairs[np.random.choice(len(best_pairs))][0]
        
        ### Place new qubit q at the right place in keep_qubits
        for j in range(len(keep_qubits)):
            if keep_qubits[j]>q:
                keep_qubits = keep_qubits[:j] + [q] + keep_qubits[j:]
                mapping = mapping[:j] + [p] + mapping[j:]
                break
        else: ### This case is reached only if q is larger than any qubits in keep_qubits
            keep_qubits.append(q)
            mapping.append(p)   
            
    return mapping

##-------------------------------------------------------
##      Subfunction used is GRASPbuild
##-------------------------------------------------------
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

def get_neighborhood_circ(circuit):
    n = circuit.num_qubits
    Neighborhood_circ = {i:set() for i in range(n)}
    for instruction in circuit.data:
        nb_qubits = len(instruction.qubits)
        if nb_qubits == 2:
            qubits = tuple((int(circuit.qubits.index(instruction.qubits[i])) for i in range(nb_qubits)))
            i,j = qubits
            Neighborhood_circ[i].add(j)
            Neighborhood_circ[j].add(i)
    return Neighborhood_circ

def get_init_qubit_list(circuit):
    n = circuit.num_qubits
    prio = {i:0 for i in range(n)}
    for instruction in circuit.data:
        nb_qubits = len(instruction.qubits)
        if nb_qubits == 2:
            qubits = tuple((int(circuit.qubits.index(instruction.qubits[i])) for i in range(nb_qubits)))
            i,j = qubits
            prio[i]+=1
            prio[j]+=1
    return sorted(prio, key=prio.get, reverse=True)


##-------------------------------------------------------
##      Fitness functions
##-------------------------------------------------------
### Won't work in GRASP because I used distance matrix (Dist) in GRASPbuild instead of backend
### Necessary for post-GRASP evaluation and comparison
def fitnessOG(qc,layout,backend):
    qr=qc.qregs[0]
    init_layout={qr[i]:layout[i] for i in range(len(layout))}
    init_layout=Layout(init_layout)

    pm = generate_preset_pass_manager(3,backend,initial_layout=init_layout)
    pm.layout.remove(1)
    pm.layout.remove(1)

    QC=pm.run(qc)
    # print(QC.count_ops())
    return QC.depth()

# def fitness(circuit, mapping, Dist):
#     # Dist = backend.coupling_map.distance_matrix
#     circ_dic = get_circuit_dic(circuit)
#     circ_dic = SABRE_routing(Dist,circ_dic,mapping)
#     return len(circ_dic)

# ### Usefull for testing
# def fitnessdraw(circuit, mapping, Dist):
#     # Dist = backend.coupling_map.distance_matrix
#     circ_dic = get_circuit_dic(circuit)
#     circ_dic = SABRE_routing(Dist,circ_dic,mapping)
#     return circ_dic

def fitness_cpp(circuit, mapping, Dist):
    qc_gates = get_circuit_array(circuit)
    depth_batch, num_gates_batch = routing.circuit_routing_batch(qc_gates, Dist, [mapping])
    return depth_batch[0]

def fitness_cpp_swap(circuit, mapping, Dist):
    qc_gates = get_circuit_array(circuit)
    depth_batch, num_gates_batch = routing.circuit_routing_batch(qc_gates, Dist, [mapping])
    return depth_batch[0], num_gates_batch[0]

##-------------------------------------------------------
##      Local search of the GRASP algorithm
##-------------------------------------------------------
### Local search design using the matrix of interaction. The goal is to bring qubits closer if they interact a lot
def GRASPlocal_search(circuit, mapping, cost, nbgate, Dist, cost_func):
    mapping = [int(i) for i in mapping]
    best_neighbor = mapping.copy()
    best_cost = cost
    best_nbgate = nbgate
    change = False
    
    ### Create the interaction frenquency matrix
    nb_qubit_circ = len(mapping)
    frequency_matrix = np.zeros([nb_qubit_circ,nb_qubit_circ])    
    for instruction in circuit.data:
        nb_qubits = len(instruction.qubits)
        if nb_qubits == 2: 
            qubits = tuple((int(circuit.qubits.index(instruction.qubits[i])) for i in range(nb_qubits)))
            i,j = qubits
            frequency_matrix[i,j] += 1    
            frequency_matrix[j,i] += 1
    
    ### Use frequency matrix to determine which qubit should be brought closer  
    flat_indices = np.argsort(frequency_matrix.ravel())[::-1]
    nonzero_indices = flat_indices[frequency_matrix.ravel()[flat_indices] != 0]    
    pair_order = [
        tuple(map(int, np.unravel_index(i, frequency_matrix.shape)))
        for i in nonzero_indices
    ]

    
    ### Run the test and improve mapping
    for pair in pair_order:
        l0,l1 = pair
        if l0 == l1: continue ## Skip all (i,i) pair
        q0 = mapping[l0]
        S = set(s for s in range(len(Dist)) if (Dist[q0,s] == 1 ))
        for s in S:
            if mapping[l1] == s: continue ## mapping would not change !
            neighbor = mapping.copy()
            if s in mapping:
                neighbor[mapping.index(s)] = mapping[l1]
            neighbor[l1] = s ## s is a physical qubit next to q0. Move l1 next to it
            
            cost, nbgate = cost_func(circuit, neighbor, Dist)
            # print(neighbor, pair)
            
            
            if cost < best_cost:
                best_cost = cost
                best_nbgate = nbgate
                best_neighbor = neighbor
                change = True
                # print('lol')
            elif cost == best_cost and nbgate < best_nbgate:
                best_cost = cost
                best_nbgate = nbgate
                best_neighbor = neighbor
                change = True
                # print('ping')
        if change: ### This pair improved the mapping.
            break
    
    return best_neighbor, best_cost, best_nbgate, change

