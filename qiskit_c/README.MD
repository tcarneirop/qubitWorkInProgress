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

### Build and install the patched Python wheel

After applying the same Rust patch used for `make c`, activate the virtual environment above and run:

```bash
cd ~/qubitWorkInProgress/qiskit
QISKIT_BUILD_PROFILE=release python -m pip wheel . --no-deps -w /tmp/qiskit-patched
unzip -p /tmp/qiskit-patched/qiskit-*.whl qiskit/capi/include/qiskit/types.h | grep -n -C 3 num_trials
python -m pip install --force-reinstall --no-deps /tmp/qiskit-patched/qiskit-*.whl
cd ~/qubitWorkInProgress/qiskit_c
grep -n -C 3 num_trials "$VIRTUAL_ENV/lib/python3.12/site-packages/qiskit/capi/include/qiskit/types.h"
make sanity_trials
./sanity_trials.exe
```

Use an empty wheel output directory (or select the exact wheel). The tested artifact was `qiskit-2.6.2-cp310-abi3-linux_x86_64.whl`; the runtime test returned `Default num_trials: 5`. The upstream `pyproject.toml` uses `setuptools.build_meta` with `setuptools-rust`; `setup.py` builds `qiskit._accelerate` from `crates/pyext/Cargo.toml` and generates the C API headers in `qiskit.capi`. No additional Python routing API patch is needed: native C/Rust calls through `qk_import()` use this patched extension. Do not replace the local wheel with the stock package in subsequent installations.

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

Washington was validated with 127 physical qubits, `dt = 2.2222222222222221e-10 s`, and CX(0,1) duration `8.533333333333332e-07 s`. These are fake-backend snapshot properties, not live calibration data. Albatroz is not listed by this provider and remains a custom target. The Python-backed benchmark now supports dynamic backend selection through `--topology NAME`.

### C++ integration

The headers must come from the Python package, not `dist/c/include`. Define `QISKIT_PYTHON_EXTENSION` before including `qiskit.h`, initialize Python, and call `qk_import()` in every translation unit that uses this API before calling its functions. Create the Python `Target`, obtain its `PyObject*`, and call `qk_target_borrow_from_python()`. The borrowed target remains owned by Python; keep the Python object alive and do not call `qk_target_free()` on it.

The validated `sanity_python_target.cpp` uses this approach. The current Makefile already contains this rule:

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

**Important:** Do not use the standalone `INCLUDES` or `LDFLAGS` in this rule. Building the standalone library with `python_binding` exported the borrowing function but failed at runtime with `TypeError: 'Target' object is not an instance of 'BaseTarget'`. The Python-backed API resolved this failure. The stock PyPI Qiskit package does not contain our custom SABRE `num_trials` patch. Build and install a local wheel from the same patched source (see below). Both patched build paths have been validated independently. Do not pass native objects between these two API builds.

### Setup and sanity scripts

```bash
bash setup_qiskit.sh
bash sanity_check.sh
```

Setup preserves the original patched standalone build, installs the pinned Python dependencies, builds and installs the patched local wheel, and runs the standalone, Python-backed and Scheduling sanity checks. It reuses `QISKIT_VENV` if specified, otherwise the active `VIRTUAL_ENV`, otherwise creates `.venv` in this project. For a separate invocation of the sanity script, activate the same environment or set `QISKIT_VENV` to its path. The Python-backed sanity checks do not require the Makefile targets; the Scheduling check includes the project’s existing `scheduling_utils.hpp`.

## Build the C++ examples

The project's Makefile uses the following configuration:

```makefile
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra
QISKIT_HOME ?= $(HOME)/qubitWorkInProgress/qiskit
INCLUDES = -I$(QISKIT_HOME)/dist/c/include
LIBDIR = $(QISKIT_HOME)/dist/c/lib
LDFLAGS = -L$(LIBDIR) -Wl,-rpath,$(LIBDIR) -lqiskit
```

The current Makefile provides the following targets. Run them from the `qiskit_c` directory.

| Target | Output | Behavior |
| --- | --- | --- |
| `sanity` | `sanity_test.exe` | Build and run standalone sanity test. |
| `circuit` | `circuit_test.exe` | Build and run circuit test. |
| `target` | `target_test.exe` | Build and run target test. |
| `transpile` | `transpile_test.exe` | Build and run transpilation test. |
| `mapping` | `mapping_test.exe` | Build and run mapping test. |
| `qasm` | `qasm_test.exe` | Build and run with `test.qasm`. |
| `mapping-generic` | `mapping_generic_test.exe` | Build generic mapping test. |
| `sabre` | `sabre_test.exe` | Build SABRE test. |
| `sabre-bench` | `sabre_benchmark.exe` | Build standalone SABRE benchmark. |
| `heuristics` | `heuristics.exe` | Build heuristics driver. |
| `sanity_trials` | `sanity_trials.exe` | Check the patched default trial count via `qk_import()`. |
| `heuristics-python` | `heuristics_python.exe` | Build dynamic-target heuristics with optional Scheduling. |
| `sanity_python` | `sanity_python` | Build Python-backed sanity test. |
| `topology` | `topology_test.exe` | Build dynamic topology test. |
| `sabre-bench-topology` | `sabre_bench_topology.exe` | Build Python-backed SABRE benchmark. |
| `clean` | — | Remove `*.exe` files (not `sanity_python`). |

The build targets that use Python require an active Python 3.12 virtual environment and use the installed package's C API headers and Python embedding flags. They do not link the standalone Qiskit library. `QISKIT_HOME` can be overridden for standalone targets.

The current Makefile no longer defines `two-opt`; historical examples below are retained for reference, not as current build commands. The Python-backed driver interface is documented below.

## Dynamic topology benchmark

```bash
source ~/qubitWorkInProgress/GRASP_to_translate/.venv/bin/activate
cd ~/qubitWorkInProgress/qiskit_c
make topology
./topology_test.exe cairo
./topology_test.exe inexistente
make sabre-bench-topology
./sabre_bench_topology.exe ../NEW_Bechmark/sym9_146.qasm --topology washington --runs 1 --seed 42
```

`--topology NAME` defaults to `albatroz`. Albatroz is constructed locally; other names are looked up as `fake_NAME` through `FakeProviderForBackendV2`, without a hard-coded list. Unknown names produce an error (the isolated topology test returned exit status 1). The benchmark checks the logical-qubit count against the selected target's physical-qubit count.

`topology_utils.hpp` returns a `PythonTarget` containing a borrowed `QkTarget*` and its owning Python reference. The benchmark uses an ownership-aware RAII deleter: locally created targets are freed with `qk_target_free()`, whereas borrowed targets release their Python reference with `Py_DECREF()`. All Qiskit objects must be destroyed before `Py_Finalize()`. Copying the Python target with `qk_target_copy()` caused a PyO3 panic in the tested embedding configuration and is not used.

### Translation, optimization and scheduling

```bash
./sabre_bench_topology.exe ../NEW_Bechmark/sym9_146.qasm --topology washington --runs 1 --seed 42 --pipeline --optimization-level 0
./sabre_bench_topology.exe ../NEW_Bechmark/sym9_146.qasm --topology washington --runs 1 --seed 42 --pipeline --optimization-level 1
```

Without `--pipeline`, the benchmark executes SABRE Layout + Routing. With `--pipeline`, it additionally executes Translation, Optimization and Scheduling (ASAP by default; select `--scheduling alap`). A Python-owned target is required; Albatroz is rejected. The reported `swaps` count is collected before Translation. `seconds` measures Layout + Routing; `pipeline_seconds` measures elapsed time from the start of SABRE Layout + Routing through Scheduling and makespan estimation. Gate/depth metrics come from the optimized circuit before delay padding. `makespan_seconds` is the estimated circuit duration on the target, not a host execution time or measured QPU runtime.

Validated Washington results for `sym9_146.qasm` (12 logical qubits, 378 original gates, seed 42):

| Optimization level | Gates | Depth | Pre-translation SWAPs |
| --- | ---: | ---: | ---: |
| 0 | 615 | 382 | 79 |
| 1 | 587 | 364 | 79 |

Layout + Routing was also validated with Cairo (27 physical qubits) and Albatroz (16). These are functional tests, not controlled comparisons between hardware targets. The exact passes controlled by `optimization_level` remain to be investigated.

## Project components

| File | Responsibility |
| --- | --- |
| `topology_utils.hpp` | Load IBM fake targets dynamically and retain the owning Python reference. |
| `topology_test.cpp` | Validate dynamic topology loading and unknown names. |
| `sanity_python_target.cpp` | Validate Python-backed C API access. |
| `sabre_benchmark.cpp` | Standalone SABRE benchmark. |
| `sabre_benchmark_topology.cpp` | Python-backed benchmark with dynamic targets and full pipeline. |
| `pipeline_utils.hpp` | Translation, Optimization and Scheduling for the SABRE benchmark. |
| `scheduling_utils.hpp` | Shared Python Scheduling bridge and duration estimation. |
| `heuristics_python.cpp` | Dynamic-target 2-change/3-change driver with optional Scheduling. |
| `sanity_trials.cpp` | Verify the patched C API default trial count through the Python extension. |
| `benchmark_utils.hpp` | Benchmark arguments, results and CSV utilities. |
| `heuristics.cpp` | Current heuristics build entry point; CLI not verified here. |
| `qasm_parser.hpp` | Parse the supported subset of OpenQASM 2 into an intermediate representation. |
| `qiskit_circuit.hpp` | Construct a `QkCircuit` from parsed gates and parameters. |
| `albatroz_target.hpp` | Define the 16-qubit Albatroz target, connectivity, and gate support. |
| `circuit_metrics.hpp` | Report gate counts, SWAP counts, circuit depth and store makespan in `MappingResult`. |
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

Both heuristics start with an initial random logical-to-physical assignment and evaluate neighbors with fixed-mapping SABRE Routing. The primary objective is to **minimize depth** (or makespan when Scheduling is enabled), with pre-translation SWAP count as the tie-breaker.

The complete physical mapping has one entry per physical qubit (16 for Albatroz, 27 for Montreal, 127 for Washington). **The search loops use the number of logical qubits in the input circuit**, not 16 unconditionally. For a 12-logical-qubit circuit, the candidate positions range from 0 to 11; entries 12–15 remain part of the mapping but are not exchanged by these local-search neighborhoods.

- **2-opt:** swap every pair of positions `i < j`. Number of neighbors: `L(L-1)/2`.
- **3-opt:** for every triple `i < j < k`, evaluate the five non-identity permutations of the three selected entries. Number of evaluations: `5 * C(L,3)`. Some 2-change-equivalent neighbors may recur across triples.

Each search explores **one neighborhood** of the initial mapping; it is not a recursive or repeated-improvement local search.

### Command line

```text
./heuristics_python.exe <file.qasm> <2|3> <num_trials> [optimization_level] [translate: 0|1] [optimize: 0|1] [schedule: 0|1] [topology] [asap|alap]
```

```bash
make heuristics-python
RAYON_NUM_THREADS=1 ./heuristics_python.exe ../NEW_Bechmark/sym9_146.qasm 2 5 0 0 0 0 montreal
RAYON_NUM_THREADS=1 ./heuristics_python.exe ../NEW_Bechmark/sym9_146.qasm 3 5 0 1 1 1 montreal alap
RAYON_NUM_THREADS=1 ./heuristics_python.exe ../NEW_Bechmark/sym9_146.qasm 2 5 0 1 1 1 washington asap
```

Defaults: optimization level 0, all three stage flags off, Albatroz, ASAP. The initial mapping seed is 128 and initial SABRE seed is 42 in the tested Python-backed driver. Neighbor routing seeds come from a generator initialized with `std::random_device`; the printed initial seed does not guarantee identical neighbor routing across runs. Scheduling requires both preceding flags and a Python-owned target. Albatroz supports the native stages but not Scheduling in this implementation. The full heuristic pipeline was validated at level 0 only; failures at higher levels remain under investigation. The benchmark SABRE pipeline has separately run at level 1.

The existing standalone `heuristics.exe` remains separate; its positional CLI ends at `[optimize: 0|1]`. The older `two_opt_test.exe` interfaces are historical.

### Implementation and ownership

For each mapping, `evaluate_mapping()` creates an independent physical DAG and transpiler state, runs native SABRE Routing, counts SWAPs before Translation, optionally runs Translation and Optimization, and calculates structural metrics before Scheduling. With all three stage flags enabled, `qk_dag_to_python()` transfers the physical DAG to Python; the native pointer is cleared to avoid double-free. Python-owned references are released with `Py_XDECREF`, and targets remain alive until all evaluations finish. The original logical DAG is preserved. The initial mapping is evaluated with the same flags as its neighbors. `is_better()` compares makespan when Scheduling is enabled, otherwise depth; both use SWAPs to break exact ties.

The SABRE benchmark retains its own `pipeline_utils.hpp` because its layout/state construction differs. Both paths reuse `scheduling_utils.hpp`: convert the Python DAG to a circuit, construct the scheduling pass manager using `generate_scheduling(target.durations(), method, target.timing_constraints(), target=target)`, run it, and call `estimate_duration(target, "s")`. This estimates the scheduled circuit duration from fake-target timing properties, not actual hardware execution. Native Mapping, Routing, Translation and Optimization are not replaced by Python calls.

### Example validation

For a QASM input with **12 logical qubits** on the **16-qubit Albatroz** target, the 2-opt search should examine:

```text
12 * 11 / 2 = 66 neighbors
```

A reported test evaluated 66 neighbors, improving SWAP count from `1721` to `1600`, with the best candidate corresponding to swapping logical mapping positions `(5,11)`.

For 12 logical qubits, five non-identity permutations per triple yield `5 * C(12,3) = 1,100` 3-change evaluations.

## Timing and parallel execution

Two timing metrics are retained:

- **Total SABRE Routing time:** sum of time spent within the routing calls across the initial mapping and all candidates.
- **Total local-search evaluation time:** elapsed time from evaluation of the initial mapping through completion of neighbor exploration. It includes mapping/DAG preparation, routing, result extraction, but excludes initial QASM parsing, target creation, and initial random mapping generation.

The Qiskit Rust implementation uses Rayon for parallel work. Control the number of Rayon worker threads with `RAYON_NUM_THREADS`:

```bash
RAYON_NUM_THREADS=1  ./heuristics_python.exe ../NEW_Bechmark/sym9_146.qasm 3 100 0 0 0 0 montreal
RAYON_NUM_THREADS=24 ./heuristics_python.exe ../NEW_Bechmark/sym9_146.qasm 3 100 0 0 0 0 montreal
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
- SWAP count, circuit depth, makespan (when enabled), stage flags, scheduling method, accumulated routing time, and complete evaluation time. Record the neighbor seeds for controlled comparisons; they are not currently fixed.

The `qiskit-2.6.2-sabre-routing.patch` file should be regenerated from the Qiskit checkout after any further Rust or C API changes, as described in the [patch document](QISKIT_2_6_2_SABRE_ROUTING_PATCH.md).

## Current scope

This project is an experimental framework for mapping evaluation and performance studies, rather than a complete quantum compiler. Future work includes investigating optimization-level semantics across stages, additional local-search heuristics, pruning, and systematic performance-portability experiments. The current results do not demonstrate quantum computational advantage.

## Functional validation (patched Python-backed API)

Reported runs on `sym9_146.qasm`, 12 logical qubits, level 0, five routing trials and one Rayon thread:

| Target | Search | Method | Initial makespan (s) | Best makespan (s) | Best depth | Best SWAPs | Neighbors |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| Montreal | 2-change | ASAP | 0.000140345 | 0.000120341 | 446 | 111 | 66 |
| Montreal | 2-change | ALAP | 0.000140345 | 0.000127004 | 445 | 112 | 66 |
| Montreal | 3-change | ASAP | 0.000140345 | 0.000116672 | 385 | 106 | 1100 |
| Montreal | 3-change | ALAP | 0.000140345 | 0.000115499 | 410 | 108 | 1100 |
| Washington | 2-change | ASAP | 0.00015648 | 0.000153252 | 419 | 149 | 66 |

These are functional tests, not controlled comparisons of scheduling methods or search quality. The no-Scheduling path and Albatroz rejection were also tested. Cairo Layout + Routing worked, but Translation failed in a full-pipeline test; its cause is not established. Exact optimization-level pass semantics remain open. Timing of the full heuristic includes all enabled stages and Python overhead, while accumulated Routing time measures only native Routing calls.
