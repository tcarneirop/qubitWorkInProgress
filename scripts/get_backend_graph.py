from qiskit.providers.fake_provider import Fake127QPulseV1, Fake5QV1, Fake20QV1, Fake27QPulseV1# New import for the backend
from qiskit.providers import BackendV2Converter

backend = Fake27QPulseV1()  ##Cairo
backend = BackendV2Converter(backend)

print(backend.coupling_map)
