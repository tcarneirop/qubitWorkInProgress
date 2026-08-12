# Making
```bash
make clean 
make all
```

# Executing for a specific instance:
```bash
$./qubit.exe NEW_Bechmark/sym9_146.qasm 16 0.5 1 1 
```
Where $sym9_146.asm$ is the instance, $16$ is a fixed parameter, $0.5$ is the percentage of the permutation lexicografically generated, and the last $1$ is the number of SABRE heuristic runs. 

# Qubit Experiment Runner

`experiments.sh` automates experiments with `qubit.exe` over multiple parameter combinations and stores the results in a CSV file.

## Usage

```bash
./2207experiments.sh [OPTIONS]
```

## Options

### `--binary <path>`

Path to the executable.

Example:

```bash
./2207experiments.sh --binary ./qubit.exe
```

### `--depths <list>`

Percentage of the partial permutation that is going to be fixed.

Values are specified as fractions of the partial permutation size.

Example:

```bash
--depths 0.2,0.3,0.4,0.5
```

One experiment is executed for each value.

**Warning:** `0.5` can be VERY memory demanding for some instances.

### `--pool <list>`

Pool percentages.

Example:

```bash
--pool 1,0.1,0.01,0.001
```

One experiment is executed for each pool value.

### `--sabre <list>`

Number of SABRE runs.

Example:

```bash
--sabre 1,5,10
```

### `--timelimits <list>`

Execution time limits.

Example:

```bash
--timelimits 10s,60s,1200s
```

The format follows the GNU `timeout` command.

### `--numthreads <list>`

Number of OpenMP threads.

Example:

```bash
--numthreads 1,2,4,8,16
```

One experiment is executed for each number of threads.

If this option is not specified, the default is the number of processors reported by `nproc`.

### `--likwid`

Run the executable using LIKWID to pin threads.

```bash
--likwid
```

When enabled, the script checks that `likwid-pin` is available before starting the experiments.

### `--csv <file>`

CSV file where experiment results are stored.

Example:

```bash
--csv results.csv
```

### `--continue`

Skip experiments that are already present in the CSV file.

```bash
--continue
```

This allows an interrupted experiment campaign to be resumed without re-running completed parameter combinations.

### `--help`, `-h`

Display the command-line help.

```bash
./2207experiments.sh --help
```

## Example

```bash
./2207experiments.sh \
    --binary ./qubit.exe \
    --depths 0.25,0.30 \
    --pool 1 \
    --sabre 1,10 \
    --timelimits 5s,10s,60s \
    --csv 10sresults.csv \
    --numthreads 256 \
    --continue
```

## Parameter Sweep

The script executes one experiment for every combination of the supplied parameters:

- QASM instance
- time limit
- depth
- pool percentage
- number of SABRE runs
- number of OpenMP threads

For example:

```text
--depths 0.2,0.3
--pool 1,0.1
--sabre 1,10
--timelimits 60s,120s
--numthreads 8,16
```

produces:

```text
2 × 2 × 2 × 2 × 2 = 32
```

experiments per QASM instance.

## Output

### CSV

The CSV records the parameters and the best solution information obtained during each execution:

- executable
- timeout
- instance
- number of SABRE runs
- pool percentage
- initial depth
- number of OpenMP threads
- execution time
- final depth found
- number of gates
- mapping
- number of solutions
- execution status

Even when an experiment reaches its time limit, the script extracts the last reported solution from the output.

### Raw Output

The complete output of each experiment is stored separately under:

```text
results/<date>_<parameters>/<instance>/<instance>.out
```

This preserves the complete program output for later analysis.
