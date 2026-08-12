#!/usr/bin/env python3

import argparse
import re
import matplotlib.pyplot as plt
from pathlib import Path

###############################################################################
# Parse arguments
###############################################################################

parser = argparse.ArgumentParser(
    description="Plot best solution evolution from a QUBIT log."
)

parser.add_argument(
    "logfile",
    help="Execution log"
)

parser.add_argument(
    "-t",
    "--timelimit",
    type=float,
    default=600.0,
    help="Execution time limit (seconds)"
)

parser.add_argument(
    "--instance",
    required=True,
    help="Instance name"
)

parser.add_argument(
    "--qubits",
    type=int,
    required=True,
    help="Number of physical qubits"
)

parser.add_argument(
    "--logic-qubits",
    type=int,
    required=True,
    help="Number of logical qubits"
)

parser.add_argument(
    "--depth",
    type=int,
    required=True,
    help="Initial cutoff depth "
)
parser.add_argument(
    "--sabre",
    type=int,
    required=True,
    help="Number of SABRE trials "
)

parser.add_argument(
    "--graphsols",
    action="store_true",
    help="Plot solution distribution instead of solution evolution."
)

args = parser.parse_args()

###############################################################################
# Regular expressions
###############################################################################

time_re = re.compile(r"New solution found at:\s*([0-9]*\.?[0-9]+)")
depth_re = re.compile(r"Depth:\s*(\d+)")

num_sols_re = re.compile(r"NUM SOLS:\s*(\d+)")
num_sols = None



###############################################################################
# Read log
###############################################################################

times = []
depths = []

pending_time = None

with open(args.logfile, "r", encoding="utf8", errors="ignore") as f:

    for line in f:
        m = num_sols_re.search(line)
        if m:
            num_sols = int(m.group(1))

        m = time_re.search(line)
        if m:
            pending_time = float(m.group(1))
            continue

        if pending_time is not None:

            m = depth_re.search(line)

            if m:
                times.append(pending_time)
                depths.append(int(m.group(1)))
                pending_time = None

if len(times) == 0:
    raise RuntimeError("No solutions found in the log.")

###############################################################################
# Build staircase
###############################################################################

x = times.copy()
y = depths.copy()

# extend until timeout
x.append(args.timelimit)
y.append(depths[-1])

###############################################################################
# Plot
###############################################################################

plt.style.use("tableau-colorblind10")

fig, ax = plt.subplots(figsize=(8, 4.8), dpi=300)

ax.step(
    x,
    y,
    where="post",
    linewidth=2.6,
    label="Best solution"
)

ax.scatter(
    times,
    depths,
    s=18,
    zorder=3
)

###############################################################################
# Formatting
###############################################################################

#ax.set_xscale("log")

# avoid log(0)
xmin = max(1e-1, min(times) * 0.8)

ax.set_xlim(xmin, args.timelimit)

margin = (max(depths) - min(depths)) * 0.05
if margin == 0:
    margin = 1

ax.set_ylim(
    min(depths) - margin,
    max(depths) + margin
)

ax.set_xlabel("Time (s)", fontsize=12)
ax.set_ylabel("Best circuit depth", fontsize=12)


title = (
    f"{args.instance} | "
    f"Logical qubits: {args.logic_qubits} | "
    f"Physical qubits: {args.qubits} | \n "
    f"Initial depth: {args.depth} | " 
    f"Exec time (s): {args.timelimit} |\n"
    f"Number of SABRE trials: {args.sabre}\n"
    f"Number of complete sols found: {num_sols:,}"

)


ax.set_title(title, fontsize=10)

ax.grid(
    which="minor",
    linestyle=":",
    linewidth=0.5,
    alpha=0.20
)

ax.spines["top"].set_visible(False)
ax.spines["right"].set_visible(False)

ax.legend(frameon=False)

plt.tight_layout()

basename = Path(args.instance).stem
output = f"{basename}_evolution.pdf"
plt.savefig(output)



  
###################################


if args.graphsols:

    solution_re = re.compile(r"Solution Value:\s*(\d+)")
    mapping_re  = re.compile(r"Number of mappings:\s*(\d+)")

    values = []
    counts = []

    pending = None

    with open(args.logfile, encoding="utf8", errors="ignore") as f:

        for line in f:

            m = solution_re.search(line)
            if m:
                pending = int(m.group(1))
                continue

            if pending is not None:
                m = mapping_re.search(line)
                if m:
                    values.append(pending)
                    counts.append(int(m.group(1)))
                    pending = None

    fig, ax = plt.subplots(figsize=(8,6), dpi=300)

    
    ax.barh(values, counts, height=0.8)

    ax.set_xlabel("Number of mappings")
    ax.set_ylabel("Solution value")

    ax.set_title(title)

    ax.grid(axis="x", linestyle="--", alpha=0.4)

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    plt.tight_layout()
    output = f"{basename}_solutions.pdf"
    plt.savefig(output)

    