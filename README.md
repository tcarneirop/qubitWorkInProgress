# Qubit Mapper

Qubit mapping and routing experiments for QASM circuits, with optimization of either the circuit's gate count or its depth.

## Building

```bash
make clean
make all
```

`make all` builds both executables from the **same source code**. Compile-time `-D` definitions select the optimization objective:

- **`qubit_gates.exe`** — optimize the number of gates.
- **`qubits_depth.exe`** — optimize circuit depth.

Both executables have **exactly the same CLI11 command-line interface**. The only distinction is the compile-time optimization objective.

## Running

The required inputs are a hardware topology and a QASM file. For example:

```bash
./qubit_gates.exe --topology albatroz --qasmfile NEW_Bechmark/z4_268.qasm
```

For depth optimization, use the same options with `./qubits_depth.exe`.

To see the current options directly from the executable:

```bash
./qubit_gates.exe --help
```

### Command-line options

| Option | Description |
| --- | --- |
| `--topology <name>` | **Required.** Hardware topology: `albatroz` (16 qubits), `boeblingen` (20), `cairo` (27), or `melbourne` (15). |
| `--qasmfile <path>` | **Required.** Input QASM circuit. |
| `--depth-percent <float>` | Fraction of the permutation to generate a pool or fix the Jurema search. |
| `--pool-percent <float>` | Fraction of the pool used by DFS or of the solutions pool to explore. |
| `--sabre-runs <int>` | Number of SABRE runs (default: 20). |
| `--num-rand-sols <int>` | Number of random solutions evaluated to obtain an initial upper bound (default: 100). |
| `--num-rand-sols-chosen <int>` | Number of random solutions selected for further work (default: 100). |
| `--sols-skip <uint>` | Number of complete non-improving solutions before stopping; `0` disables this condition (default: 0). |
| `--search <code>` | Search algorithm: `d` (DFS), `j` (Jurema), `k` (K-changes), `t` (tests), `r` (recursive 2-change search), `a` (random/aleatoire search). |
| `--pruning` | Enable cost-bounded routing. |
| `--plot` | Enable plotting of random solutions. |
| `--permutation <ints...>` | Evaluate the routing value of a given permutation on the selected topology. |
| `-h`, `--help` | Display command-line help. |

The `p` search code has been removed from this documentation; plotting is controlled independently by `--plot`.

### Example with search parameters

```bash
./qubit_gates.exe \
  --topology albatroz \
  --qasmfile NEW_Bechmark/z4_268.qasm \
  --search j \
  --depth-percent 0.7 \
  --sabre-runs 20
```

Use the same CLI parameters with `qubits_depth.exe` to optimize circuit depth instead of the gate count.

### Checking a given permutation

Supply the permutation elements after `--permutation` using the auxiliary script:

```bash
./gates_qubit.exe --topology "albatroz" --qasmfile NEW_Bechmark/z4_268.qasm  --permutation $(./scripts/cleanPermutation.sh "[11, 3, 0, 1, 12, 4, 15, 13, 6, 5, 8]")
```

The application evaluates its routing value using the topology specified with `--topology`. The number of elements must match the problem being evaluated.
