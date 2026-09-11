import json
from pathlib import Path

from qiskit_ibm_runtime.fake_provider import (
    FakeYorktownV2,
    FakeBoeblingenV2,
    FakeMelbourneV2,
    FakeCairoV2,
    FakeRochesterV2,
    FakeManhattanV2,
    FakeBrisbane,
    FakeTorino,
)


BACKENDS = {
    "yorktown": FakeYorktownV2,
    "boeblingen": FakeBoeblingenV2,
    "melbourne": FakeMelbourneV2,
    "cairo": FakeCairoV2,
    "rochester": FakeRochesterV2,
    "manhattan": FakeManhattanV2,
    "brisbane": FakeBrisbane,
    "torino": FakeTorino,
}


def get_config_edges(backend):
    """Read the coupling_map from the backend's conf_*.json."""

    path = Path(backend.dirname) / backend.conf_filename

    with open(path) as f:
        conf = json.load(f)

    return {
        tuple(edge)
        for edge in conf["coupling_map"]
    }


def get_backend_edges(backend):
    return {
        tuple(edge)
        for edge in backend.coupling_map.get_edges()
    }


for name, backend_class in BACKENDS.items():

    backend = backend_class()

    config_edges = get_config_edges(backend)
    backend_edges = get_backend_edges(backend)

    missing = config_edges - backend_edges
    extra = backend_edges - config_edges

    print(f"\n{name}")
    print("-" * len(name))

    print(f"qubits:        {backend.num_qubits}")
    print(f"config edges:  {len(config_edges)}")
    print(f"backend edges: {len(backend_edges)}")

    if not missing and not extra:
        print("STATUS: OK")
    else:
        print("STATUS: MISMATCH")

        if missing:
            print("missing from FakeBackend:")
            for edge in sorted(missing):
                print(f"  {edge}")

        if extra:
            print("extra in FakeBackend:")
            for edge in sorted(extra):
                print(f"  {edge}")