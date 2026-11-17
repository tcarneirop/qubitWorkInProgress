# Qiskit 2.6.2: Configurable SABRE Routing Trials

## Goal

Expose the number of **internal SABRE Routing trials** as `num_trials` through the Qiskit C API. This allows the routing stage to evaluate a fixed initial qubit mapping using a configurable number of SABRE attempts, without invoking SABRE Layout.

This change is intended for experiments in which an external algorithm (e.g. 2-opt or 3-opt) proposes the initial mapping and SABRE performs routing.

**Important distinction:** `num_trials` is the number of trials *inside one routing call*. It is not the number of mappings evaluated by 2-opt, nor the standalone SABRE Layout parameters (`max_iterations`, `num_swap_trials`, `num_random_trials`).

## Version and file locations

- Qiskit source tag: `2.6.2`
- Source checkout: `~/qubitWorkInProgress/qiskit/`
- C++ project: `~/qubitWorkInProgress/qiskit_c/`
- Changed upstream source files:
  - `crates/transpiler/src/transpiler.rs`
  - `crates/cext/src/transpiler/transpile_function.rs`
- Generated artifacts:
  - `dist/c/include/`
  - `dist/c/lib/libqiskit.so`

Keep this file and `qiskit-2.6.2-sabre-routing.patch` in the C++ project's root directory.

## Change 1: Rust routing implementation

File: `crates/transpiler/src/transpiler.rs`

1. Add `num_trials: usize` immediately after `seed: Option<u64>` in the `routing_stage(...)` signature.
2. In its level-0 branch, pass `num_trials` instead of the hard-coded `5` to `sabre::sabre_routing(...)`:

```rust
let (out_dag, final_layout) = sabre::sabre_routing(
    dag,
    &routing_target,
    sabre_heuristic,
    initial_layout,
    num_trials,
    seed,
    Some(true),
)?;
```

3. Update the `routing_stage(...)` call inside `transpile(...)` to pass `5` after `seed`. This preserves the original behavior of the full Rust transpiler.

## Change 2: C API options and routing stage

File: `crates/cext/src/transpiler/transpile_function.rs`

1. Add a field to `TranspileOptions` immediately after `seed: i64`:

```rust
/// Number of internal SABRE Routing trials.
num_trials: usize,
```

2. Set the default in `impl Default for TranspileOptions`:

```rust
TranspileOptions {
    optimization_level: 2,
    seed: -1,
    num_trials: 5,
    approximation_degree: 1.0,
}
```

3. Inside `qk_transpile_stage_routing(...)`, pass `options.num_trials` immediately after `seed` when calling `routing_stage(...)`:

```rust
routing_stage(
    dag,
    target,
    options.optimization_level.into(),
    seed,
    options.num_trials,
    &sabre_heuristic,
    out_layout,
    // Use Sabre here to ensure we run VF2PostLayout
    LayoutSource::Sabre,
)
```

**Scope:** The configurable `num_trials` is currently used by the **level-0** routing branch, which is the branch used for fixed-mapping experiments. This change does not make higher optimization levels respect an externally fixed mapping.

## Build and validation status

The customized Qiskit 2.6.2 C library **built successfully** with:

```sh
cd ~/qubitWorkInProgress/qiskit
make c
```

The generated C header was **verified**: `dist/c/include/qiskit/types.h` contains `size_t num_trials;` inside the generated transpiler options structure (line 2190 in the user's checkout).

The existing `mapping_test.cpp` was subsequently rebuilt and ran successfully with the modified library. Its observed routed circuit had 6 SWAPs, 8 instructions, and depth 6. This demonstrates that the fixed-mapping routing flow still works; it does not by itself establish a controlled performance comparison between trial counts.

The `two_opt_test.cpp` was also rebuilt and executed successfully after the wrapper refactoring. One reported 16-qubit neighborhood search evaluated 120 neighbors, improving from 3650 to 3582 SWAPs, with 0.601999 seconds accumulated routing time and 1.35217 seconds total 2-opt evaluation time. The command-line parameter support was subsequently reported working. **No controlled 5-versus-20-trial comparison has yet been reported.**

### C++ routing wrapper

File: `~/qubitWorkInProgress/qiskit_c/routing_utils.hpp`

```cpp
inline bool run_sabre_routing(
    QkDag* physical_dag,
    const QkTarget* target,
    QkTranspilerStageState* state,
    int64_t seed = 42,
    size_t num_trials = 5
)
```

Inside the function, the options passed to `qk_transpile_stage_routing(...)` are configured as:

```cpp
QkTranspileOptions options = qk_transpiler_default_options();
options.optimization_level = 0;
options.seed = seed;
options.num_trials = num_trials;
```

The `seed` argument comes **before** `num_trials`. Existing callers with three arguments continue to use the defaults. Level 0 is retained to preserve the externally specified initial mapping.

### Propagation to mapping evaluation and 2-opt

File: `mapping_utils.hpp`

The general mapping evaluator accepts trailing optional parameters:

```cpp
int64_t sabre_seed = 42,
size_t num_trials = 5
```

It forwards both to `run_sabre_routing(physical_dag, target, state, sabre_seed, num_trials)`.

File: `two_opt.hpp`

The 2-opt neighborhood function receives the same two trailing optional parameters and forwards them to `evaluate_mapping()` for each candidate. The mapping is evaluated using a fresh physical DAG and transpiler state; the source logical DAG is not mutated.

File: `two_opt_test.cpp`

The command-line syntax is:

```text
./two_opt_test.exe <file.qasm> [mapping_seed] [num_trials] [sabre_seed]
```

Defaults:

| Parameter | Default | Purpose |
| --- | ---: | --- |
| `mapping_seed` | `128` | Seeds `std::mt19937` when generating the random initial physical-qubit permutation. |
| `num_trials` | `5` | Internal SABRE Routing attempts per candidate mapping. |
| `sabre_seed` | `42` | Seeds the SABRE routing heuristic, independently of `mapping_seed`. |

Example:

```sh
cd ~/qubitWorkInProgress/qiskit_c
make two-opt
./two_opt_test.exe ../NEW_Bechmark/ising_model_16.qasm 128 20 42
```

For 16 physical qubits, one complete 2-opt neighborhood has `16 * 15 / 2 = 120` candidate mappings, plus the initial mapping: **121 calls to the C routing stage**. With `num_trials = 20`, each call requests 20 internal routing attempts, i.e. **2,420 internal attempts requested**, not 2,420 calls from C++. If a candidate already satisfies connectivity, level-0 routing may skip the SABRE search for that candidate.

The search minimizes the number of SWAPs, using circuit depth to break ties. `Total SABRE Routing time` sums only the measured `run_sabre_routing()` call durations. `Total 2-opt evaluation time` additionally includes per-mapping setup and metric extraction, but excludes initial QASM loading, target creation, and random mapping generation.

## Reapplying after a clean checkout

From the **Qiskit source checkout** at the exact `2.6.2` tag:

```sh
cd ~/qubitWorkInProgress/qiskit
git apply --check ../qiskit_c/qiskit-2.6.2-sabre-routing.patch
git apply ../qiskit_c/qiskit-2.6.2-sabre-routing.patch
make c
```

Do not apply the patch twice. `git apply --check` must succeed before applying it. If the working tree already contains these modifications, the check will fail; that does not imply the patch is invalid.

The patch was prepared from the source differences shared during this session. It has **not** been independently tested with `git apply --check` against the user's checkout; run that check on a clean Qiskit 2.6.2 checkout before relying on it for a reinstall.

## Updating the patch after subsequent modifications

Run this command in the **Qiskit source checkout** to regenerate an exact patch from the current working tree:

```sh
cd ~/qubitWorkInProgress/qiskit
git diff HEAD -- \
  crates/transpiler/src/transpiler.rs \
  crates/cext/src/transpiler/transpile_function.rs \
  > ../qiskit_c/qiskit-2.6.2-sabre-routing.patch
```

This captures only these two files and does not change the checked-out source. Commit the documentation and patch to the **C++ project** so the customized Qiskit build can be reproduced.
