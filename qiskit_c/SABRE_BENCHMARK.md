# SABRE Layout + Routing benchmark (Qiskit C API 2.6.2)

These files add a **new executable**, without changing `sabre_test.cpp`, `layout_utils.hpp`, or any earlier test:

- `benchmark_utils.hpp` — shared CLI parsing, per-run result structure, mean/sample standard deviation, CSV output.
- `sabre_benchmark.cpp` — SABRE-specific runner, circuit reconstruction, timing and metrics.

Place both files in `~/qubitWorkInProgress/qiskit_c/` beside your existing headers.

## Makefile

Add `sabre-bench` to `.PHONY` and append the following rule (the recipe line starts with a **tab**):

```make
sabre-bench:
	$(CXX) $(CXXFLAGS) $(INCLUDES) sabre_benchmark.cpp -o sabre_benchmark.exe $(LDFLAGS)
```

## Usage

```bash
make sabre-bench
./sabre_benchmark.exe ../NEW_Bechmark/test.qasm --runs 10 --seed 42 \
  --seed-step 1 --iterations 4 --swap-trials 20 --random-trials 20 \
  --csv sabre_test.csv
./sabre_benchmark.exe ../NEW_Bechmark/ising_model_16.qasm --runs 20 --seed 42 --csv ising_sabre.csv
```

All options have defaults except the QASM filename. `--seed-step 1` uses seeds 42, 43, 44, ...; `--seed-step 0` repeats the same seed across runs. The latter is useful for timing variation; it does not independently explore distinct SABRE random starts. Fixed seeds improve reproducibility but do not guarantee bitwise-identical timing or outcomes across different software/hardware environments.

## Metrics and experimental constraints

- `seconds`: wall-clock duration of the standalone SABRE Layout + Routing call only. Parsing, circuit creation, target creation, counts and CSV writing are excluded.
- `swaps`: number of `swap` operations in the resulting circuit, **before** any subsequent decomposition of SWAPs into basis gates.
- `instructions`, `gates`, `depth`: measured on the post-SABRE physical circuit, using the project’s existing `circuit_metrics.hpp` helpers.
- `best`: minimum SWAP count, with depth then elapsed time used as tie-breakers.
- `sample_stddev`: sample standard deviation (denominator `N-1`); zero for a single run.
- A clean `QkCircuit` is reconstructed from `ParsedCircuit` before **every** invocation: the standalone pass modifies its input in place.
- Same Albatroz target and SABRE parameters for all runs. No `optimization_level = 3`, gate optimization or scheduling is performed.
- The benchmark currently assumes circuits supported by `qasm_parser.hpp` and `qiskit_circuit.hpp` and at most 16 logical qubits.
- A CSV includes one row per run: `run,seed,seconds,instructions,gates,swaps,depth`.
- For comparative experiments also record Qiskit version, CPU, Rayon thread count (`RAYON_NUM_THREADS`), compiler/build flags and machine load; do not draw conclusions from one run.

## Reusing for other algorithms

Keep `benchmark_utils.hpp`. Replace only the function invoked in the timed region and the algorithm-specific metrics in `sabre_benchmark.cpp`. The shared CLI/result data types can be extended with additional options and metrics as required.

## Validation

The code was prepared against the public Qiskit C API signatures and the existing project interfaces discussed in this conversation, but **could not be compiled in this environment** because the user's local Qiskit 2.6.2 headers/library and helper headers were not available here. Compile and test with `make sabre-bench` locally before relying on results.

## Dynamic-target benchmark and full pipeline

The preceding sections describe the original standalone executable. The separate Python-backed executable uses the locally patched Qiskit wheel (see README and patch documentation):

```bash
make sabre-bench-topology
./sabre_bench_topology.exe ../NEW_Bechmark/sym9_146.qasm --topology washington --runs 3 --seed 42 --pipeline --optimization-level 1 --scheduling asap --csv washington.csv
```

Without `--pipeline`, only SABRE Layout + Routing runs. With it, `pipeline_utils.hpp` performs Translation, Optimization and Scheduling via the shared `scheduling_utils.hpp`. `--scheduling` accepts `asap` (default) or `alap`. The full pipeline rejects Albatroz because it requires a Python target. Other topology names are resolved dynamically through `FakeProviderForBackendV2`. The routing SWAP count is collected before translation; gates, instructions and depth are measured on the optimized circuit before scheduling delays. `seconds` measures Layout + Routing; `pipeline_seconds` measures from the same start through Scheduling and makespan estimation, including intermediate work. `makespan_seconds` estimates the target circuit duration, not actual device runtime. Zero pipeline/makespan values indicate those stages were not requested. CSV/terminal rows in the latest validated version use `run,seed,seconds,pipeline_seconds,instructions,gates,swaps,depth,makespan_seconds`. Best structural result is selected by depth, SWAPs, then time; a separate best makespan is reported when scheduling is active.

Washington (127 qubits) and Montreal (27 qubits) completed full-pipeline tests. Washington seed 42 at level 1 returned 587 gates, depth 364, 79 routing SWAPs and estimated makespan 0.00012519111111111136 s in the reported ASAP run. Cairo failed during Translation in one full-pipeline test; the cause remains unknown. The pipeline was extracted to `pipeline_utils.hpp` and the user reported successful validation. These tests are not a controlled hardware or scheduling comparison. The heuristics use a separate mapping/routing flow and reuse only the Scheduling helper.
