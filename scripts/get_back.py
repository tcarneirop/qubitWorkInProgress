from qiskit_ibm_runtime import QiskitRuntimeService

service = QiskitRuntimeService()
backend = service.backend("ibm_brisbane")

edges = backend.coupling_map.get_edges()
print(edges)
