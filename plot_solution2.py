#!/usr/bin/env python3

import argparse
import re
import matplotlib.pyplot as plt

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
    "-o",
    "--output",
    default="solution_evolution.pdf",
    help="Output figure (pdf/png/svg...)"
)

parser.add_argument(
    "-t",
    "--timelimit",
    type=float,
    default=600.0,
    help="Execution time limit (seconds)"
)

args = parser.parse_args()

###############################################################################
# Regular expressions
###############################################################################

time_re = re.compile(r"New solution found at:\s*([0-9]*\.?[0-9]+)")
depth_re = re.compile(r"Depth:\s*(\d+)")

###############################################################################
# Read log
###############################################################################

times = []
depths = []

pending_time = None

with open(args.logfile, "r", encoding="utf8", errors="ignore") as f:

    for line in f:

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

ax.set_xscale("log")

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

ax.set_title("Evolution of the Best Solution", fontsize=13)

ax.grid(
    which="major",
    linestyle="--",
    linewidth=0.7,
    alpha=0.45
)

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

plt.savefig(args.output, bbox_inches="tight")
plt.show()
