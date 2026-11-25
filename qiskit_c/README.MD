# Qiskit C API — Qubit Mapping and Routing Experiments

A C++ experimental project using the **Qiskit 2.6.2 C API** to study quantum-circuit placement, SABRE routing, and combinatorial search over logical-to-physical qubit mappings.

The initial hardware target is **Albatroz**, a 16-physical-qubit topology. The project supports reading OpenQASM 2 circuits, evaluating user-specified mappings with SABRE Routing, comparing with standalone SABRE Layout + Routing, and exploring mappings using **non-recursive 2-opt and 3-opt neighborhoods**.

## Requirements

- A C++17 compiler (tested with `g++`).
- Rust and Cargo, to build the Qiskit C library from source.
- The build tools required by Qiskit's `make c` target, including the Qiskit C header generator.
- Qiskit source code checked out at **tag `2.6.2`**.

The development setup used Rust `1.99.1`, Cargo `1.99.1`, and cbindgen `0.29.4`. These record the tested environment; they are not asserted to be strict minimum supported versions.

### Directory layout

The documented local arrangement is:

```text
~/qubitWorkInProgress/
├── qiskit/               # Qiskit 2.6.2 source checkout
│   └── dist/c/
│       ├── include/      # Generated C API headers
│       └── lib/
│           └── libqiskit.so
└── qiskit_c/             # This C++ project
    ├── Makefile
    ├── README.md
    ├── QISKIT_2_6_2_SABRE_ROUTING_PATCH.md
    └── qiskit-2.6.2-sabre-routing.patch
```

## Build the Qiskit C library

From the Qiskit source checkout:

```bash
cd ~/qubitWorkInProgress/qiskit
git checkout 2.6.2
make c
```

`make c` generates the C headers and the shared library, including:

```text
dist/c/include/qiskit.h
dist/c/include/qiskit/types.h
dist/c/lib/libqiskit.so
```

**Important:** Mapping experiments use a small local patch to expose the number of internal SABRE Routing trials. Apply the patch **before** building if using a clean, unmodified Qiskit checkout. See [QISKIT_2_6_2_SABRE_ROUTING_PATCH.md](QISKIT_2_6_2_SABRE_ROUTING_PATCH.md) for the exact changes, application procedure, and reproducibility notes. Do not apply the patch twice.

The generated `QkTranspileOptions` structure must contain `size_t num_trials;` (in `dist/c/include/qiskit/types.h`). Verify with:

```bash
grep -Rn -B 5 -A 5 'num_trials' ~/qubitWorkInProgress/qiskit/dist/c/include/
```

## Python-backed C API and IBM fake backends

The standalone, locally patched C library and the Python-backed C API are **separate build paths**. The latter uses the C API embedded in the installed Qiskit Python extension (`qiskit._accelerate`) through its function-pointer tables. It does not link the standalone `libqiskit.so`.

### Python environment

The tested environment was Python 3.12.3, Qiskit 2.6.2 and `qiskit-ibm-runtime` 0.52.1. Python development headers, the embedding library, `python3.12-config`, and virtual-environment support are required. On Ubuntu these are provided by the corresponding Python development and venv packages.

To reuse the tested environment:

```bash
source ~/qubitWorkInProgress/GRASP_to_translate/.venv/bin/activate
cd ~/qubitWorkInProgress/qiskit_c
python -m pip install 'qiskit==2.6.2' 'qiskit-ibm-runtime==0.52.1'
python -c 'import sys, qiskit, qiskit_ibm_runtime; print(sys.version); print(qiskit.__version__); print(qiskit_ibm_runtime.__version__); print(qiskit.__file__)'
python3-config --includes
python3-config --embed --ldflags
```

Alternatively, create a local environment with `python3.12 -m venv .venv`, activate it and install the same pinned packages. Do not run imports from inside the Qiskit source checkout, since the local source tree may shadow the installed package.

### Accessing a fake backend

```python
from qiskit_ibm_runtime.fake_provider import FakeProviderForBackendV2

provider = FakeProviderForBackendV2()
for backend in provider.backends():
    print(backend.name, backend.num_qubits)

backend = provider.backend('fake_washington')
target = backend.target
print(target.num_qubits, target.dt)
print(target['cx'][(0, 1)])
```

Washington was validated with 127 physical qubits, `dt = 2.2222222222222221e-10 s`, and CX(0,1) duration `8.533333333333332e-07 s`. These are fake-backend snapshot properties, not live calibration data. Albatroz is not listed by this provider and remains a custom target. The integration of arbitrary backend names into the project's `--topology` argument is future work.

### C++ integration

The headers must come from the Python package, not `dist/c/include`. Define `QISKIT_PYTHON_EXTENSION` before including `qiskit.h`, initialize Python, and call `qk_import()` in every translation unit that uses this API before calling its functions. Create the Python `Target`, obtain its `PyObject*`, and call `qk_target_borrow_from_python()`. The borrowed target remains owned by Python; keep the Python object alive and do not call `qk_target_free()` on it.

The validated `sanity_python_target.cpp` uses this approach. Add this rule to the existing Makefile:

```makefile
sanity_python: sanity_python_target.cpp
	$(CXX) $(CXXFLAGS) \
		-I$(VIRTUAL_ENV)/lib/python3.12/site-packages/qiskit/capi/include \
		$(shell python3-config --includes) \
		$< -o $@ \
		$(shell python3-config --embed --ldflags)
```

With the virtual environment active:

```bash
make -B sanity_python
./sanity_python
```

Expected output (the provider may also emit a warning about Nighthawk):

```text
Qubits: 127
dt: 2.22222e-10
```

If embedded Python cannot find the virtual environment packages, run from the project directory with `PYTHONPATH="$VIRTUAL_ENV/lib/python3.12/site-packages" ./sanity_python`.

**Important:** Do not use the standalone `INCLUDES` or `LDFLAGS` in this rule. Building the standalone library with `python_binding` exported the borrowing function but failed at runtime with `TypeError: 'Target' object is not an instance of 'BaseTarget'`. The Python-backed API resolved this failure. The pip-installed Qiskit package does **not** contain our custom SABRE `num_trials` patch; the existing patched standalone examples remain separate. A unified patched Python-backed build has not yet been validated. Do not pass native objects between these two API builds.

### Setup and sanity scripts

```bash
bash setup_qiskit.sh
bash sanity_check.sh
```

Setup preserves the original patched standalone build, installs the pinned Python dependencies, and runs both sanity checks. It reuses `QISKIT_VENV` if specified, otherwise the active `VIRTUAL_ENV`, otherwise creates `.venv` in this project. For a separate invocation of the sanity script, activate the same environment or set `QISKIT_VENV` to its path. The Python-backed sanity check is self-contained and does not require the Makefile target.

## Build the C++ examples

The project's Makefile uses the following configuration:

```makefile
CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra
QISKIT_HOME ?= $(HOME)/qubitWorkInProgress/qiskit
INCLUDES = -I$(QISKIT_HOME)/dist/c/include
LIBDIR = $(QISKIT_HOME)/dist/c/lib
LDFLAGS = -L$(LIBDIR) -Wl,-rpath,$(LIBDIR) -lqiskit
```

From the C++ project directory, the previously validated targets include:

```bash
cd ~/qubitWorkInProgress/qiskit_c
make sanity
make circuit
make target
make transpile
make mapping
make qasm
make sabre
make mapping-generic
make two-opt
```

Targets must be present in your local Makefile; not all are necessarily included in every intermediate revision. The `QISKIT_HOME` variable can be overridden to point to a different Qiskit source checkout.

## Project components

| File | Responsibility |
| --- | --- |
| `qasm_parser.hpp` | Parse the supported subset of OpenQASM 2 into an intermediate representation. |
| `qiskit_circuit.hpp` | Construct a `QkCircuit` from parsed gates and parameters. |
| `albatroz_target.hpp` | Define the 16-qubit Albatroz target, connectivity, and gate support. |
| `circuit_metrics.hpp` | Report gate counts, SWAP counts, and circuit depth. |
| `mapping_utils.hpp` | Apply a user-supplied mapping, create the transpiler layout/state, and evaluate routed mappings. |
| `routing_utils.hpp` | Invoke SABRE Routing with a fixed initial mapping, seed, and configurable internal trial count. |
| `layout_utils.hpp` | Invoke standalone SABRE Layout + Routing, which chooses its own initial mapping. |
| `two_opt.hpp` | Explore one non-recursive 2-opt neighborhood. |
| `three_opt.hpp` | Explore one non-recursive 3-position permutation neighborhood. |
| `mapping_test.cpp` | Small fixed-mapping routing example. |
| `mapping_generic_test.cpp` | Check generic DAG mapping and routing on a QASM circuit. |
| `sabre_test.cpp` | Standalone SABRE Layout + Routing experiment. |
| `two_opt_test.cpp` | Command-line driver for local-search experiments; the latest tested invocation selects heuristic 2 or 3. |

Some examples and helper files are intermediate experiments retained for verification. Their inclusion and filenames may differ across local revisions.

## Input circuit format

The current custom parser targets the supported subset of **OpenQASM 2.0**. The validated gate set includes `h`, `x`, `sx`, `cx`, and parameterized `rz`, including expressions such as `pi/2` and `3*pi/4`.

For example:

```qasm
OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
h q[0];
x q[1];
cx q[0],q[2];
rz(3*pi/4) q[2];
cx q[1],q[2];
```

The parser is **not** a full OpenQASM implementation. Unsupported instructions and circuit features should be validated before benchmarking. Current mapping utilities are intended for circuits without classical operations.

## Standalone SABRE Layout + Routing

This route lets Qiskit choose its own initial mapping and perform routing:

```bash
make sabre
./sabre_test.exe ../NEW_Bechmark/test.qasm
```

`run_sabre_layout()` provides the following optional parameters:

| Parameter | Default | Meaning |
| --- | ---: | --- |
| `seed` | `42` | Random seed. |
| `max_iterations` | `4` | Forward–backward layout refinement iterations. |
| `num_swap_trials` | `20` | Routing attempts during layout search. |
| `num_random_trials` | `20` | Random-layout search attempts. |

**Do not use standalone SABRE Layout to evaluate a particular fixed mapping.** This route searches for its own layout.

## Fixed-mapping SABRE Routing

For mappings supplied by an external search algorithm, the evaluation pipeline is:

1. Create the logical `QkCircuit` and `QkDag` from the QASM input.
2. Build an independent physical DAG using the candidate mapping.
3. Create the `QkTranspilerStageState` containing the fixed initial layout.
4. Run SABRE Routing at `optimization_level = 0`.
5. Collect SWAP count, routed depth, and timing; release the temporary Qiskit objects.

The wrapper interface is:

```cpp
run_sabre_routing(physical_dag, target, state, sabre_seed, num_trials);
```

Defaults are `sabre_seed = 42` and `num_trials = 5`. The configurable `num_trials` field is **not part of the unmodified Qiskit 2.6.2 C API**; the local Rust/C patch is required. `num_trials` requests multiple internal SABRE Routing attempts **within one call**, not multiple calls from C++.

Each candidate is evaluated using a fresh physical DAG and routing state. The original logical DAG is preserved.

## Non-recursive 2-opt and 3-opt

Both heuristics start with an initial random logical-to-physical assignment and evaluate neighbors with fixed-mapping SABRE Routing. The primary objective is to **minimize SWAPs**, with routed depth as the tie-breaker.

The complete physical mapping has 16 entries for Albatroz. **The search loops use the number of logical qubits in the input circuit**, not 16 unconditionally. For a 12-logical-qubit circuit, the candidate positions range from 0 to 11; entries 12–15 remain part of the mapping but are not exchanged by these local-search neighborhoods.

- **2-opt:** swap every pair of positions `i < j`. Number of neighbors: `L(L-1)/2`.
- **3-opt:** for every triple `i < j < k`, evaluate the six permutations of the three selected entries, including the unchanged permutation. Number of evaluations in this implementation: `6 * C(L,3)`. This deliberately retains redundant evaluations, matching the original custom implementation.

Each search explores **one neighborhood** of the initial mapping; it is not a recursive or repeated-improvement local search.

### Command line

The tested heuristic-selection interface is:

```text
./two_opt_test.exe <file.qasm> <heuristic> [mapping_seed] [num_trials] [sabre_seed]
```

For example:

```bash
make two-opt
./two_opt_test.exe ../NEW_Bechmark/cycle10_2_110.qasm 2 4 1 34
./two_opt_test.exe ../NEW_Bechmark/cycle10_2_110.qasm 3 4 20 34
```

Here `heuristic` is `2` for 2-opt or `3` for 3-opt. The mapping seed initializes the random physical permutation; `num_trials` controls the number of **internal** routing attempts per candidate; `sabre_seed` controls SABRE's randomization. Defaults used during development were `mapping_seed = 128`, `num_trials = 5`, and `sabre_seed = 42`.

Some intermediate revisions of `two_opt_test.cpp` accepted only the 2-opt-specific argument sequence (`<file.qasm> [mapping_seed] [num_trials] [sabre_seed]`). Use the executable's current usage output to confirm its interface if working from an earlier revision.

### Example validation

For a QASM input with **12 logical qubits** on the **16-qubit Albatroz** target, the 2-opt search should examine:

```text
12 * 11 / 2 = 66 neighbors
```

A reported test evaluated 66 neighbors, improving SWAP count from `1721` to `1600`, with the best candidate corresponding to swapping logical mapping positions `(5,11)`.

For 12 logical qubits, six permutations per triple yield `6 * C(12,3) = 1,320` 3-opt evaluations. This count includes redundant candidates by design.

## Timing and parallel execution

Two timing metrics are retained:

- **Total SABRE Routing time:** sum of time spent within the routing calls across the initial mapping and all candidates.
- **Total local-search evaluation time:** elapsed time from evaluation of the initial mapping through completion of neighbor exploration. It includes mapping/DAG preparation, routing, result extraction, and reporting, but excludes initial QASM parsing, target creation, and initial random mapping generation.

The Qiskit Rust implementation uses Rayon for parallel work. Control the number of Rayon worker threads with `RAYON_NUM_THREADS`:

```bash
RAYON_NUM_THREADS=1  ./two_opt_test.exe ../NEW_Bechmark/cycle10_2_110.qasm 3 4 100 34
RAYON_NUM_THREADS=24 ./two_opt_test.exe ../NEW_Bechmark/cycle10_2_110.qasm 3 4 100 34
```

In one reported single-routing comparison with 100 internal trials, the serial timing was approximately `0.0291218 s` and the parallel timing approximately `0.0047477 s`, corresponding to about `6.13x` observed speedup. These are preliminary single measurements, not a statistically controlled scaling study. Increasing the Rayon pool above the 24 available CPUs did not produce an observed performance improvement in that test.

The routing step can be skipped by the level-0 stage when the mapping already respects target connectivity. Consequently, the requested internal trial count should not automatically be interpreted as the number of trials actually executed for every candidate.

## Reproducibility

For comparable experiments, keep the following fixed or record them explicitly:

- Qiskit source version and the exact applied C API patch.
- API build path (patched standalone or Python-backed), Python and IBM Runtime versions, and fake-backend snapshot when applicable.
- QASM input and target topology.
- Initial mapping seed and SABRE seed.
- Number of internal SABRE trials.
- Selected heuristic and the logical-qubit count.
- Rayon thread count and CPU configuration.
- SWAP count, circuit depth, accumulated routing time, and complete evaluation time.

The `qiskit-2.6.2-sabre-routing.patch` file should be regenerated from the Qiskit checkout after any further Rust or C API changes, as described in the [patch document](QISKIT_2_6_2_SABRE_ROUTING_PATCH.md).

## Current scope

This project is an experimental framework for mapping evaluation and performance studies, rather than a complete quantum compiler. Future work may include integrating additional local-search heuristics, pruning, more hardware targets, and systematic performance-portability experiments. The current results do not demonstrate quantum computational advantage.
