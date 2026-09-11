import inspect
import json
from pathlib import Path

import qiskit_ibm_runtime.fake_provider as fake_provider


def get_config_edges(backend):
    path = Path(backend.dirname) / backend.conf_filename

    with open(path) as f:
        conf = json.load(f)

    return {tuple(edge) for edge in conf["coupling_map"]}


def get_backend_edges(backend):
    return {
        tuple(edge)
        for edge in backend.coupling_map.get_edges()
    }


fake_classes = {
    name: cls
    for name, cls in vars(fake_provider).items()
    if name.startswith("Fake")
    and inspect.isclass(cls)
    and hasattr(cls, "conf_filename")
    and hasattr(cls, "dirname")
}


for name, backend_class in sorted(fake_classes.items()):

    try:
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
                print("missing:")
                for edge in sorted(missing):
                    print(f"  {edge}")

            if extra:
                print("extra:")
                for edge in sorted(extra):
                    print(f"  {edge}")

    except Exception as e:
        print(f"\n{name}")
        print("-" * len(name))
        print(f"STATUS: ERROR")
        print(f"{type(e).__name__}: {e}")