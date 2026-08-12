#!/usr/bin/env python3

import argparse
import re
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(
    description="Plot solution evolution from QUBIT logs."
)
parser.add_argument("logfile")
parser.add_argument("-o", "--output", default="solution_evolution.pdf")
parser.add_argument("-t", "--timelimit", type=float, default=600)
args = parser.parse_args()

# ----------------------------------------------------------------------
# Parse log
# ----------------------------------------------------------------------

time_re = re.compile(r"New solution found at:\s*([0-9.]+)")
depth_re = re.compile(r"\s*Depth:\s*(\d+)")

times = []
depths = []

with open(args.logfile) as f:
    pending_time = None

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

if not times:
    raise RuntimeError("No solutions found.")

# ----------------------------------------------------------------------
# Build staircase curve
# ----------------------------------------------------------------------

x = [times[0]]
y = [depths[0]]

for i in range(1, len(times)):
    x.append(times[i])
    y.append(depths[i])

# Extend until time limit
x.append(args.timelimit)
y.append(depths[-1])

# ----------------------------------------------------------------------
# Plot
# ----------------------------------------------------------------------

plt.figure(figsize=(8,4.5))

plt.step(
    x,
    y,
    where="post",
    linewidth=2.5,
    color="tab:blue",
    label="Best solution"
)

plt.scatter(
    times,
    depths,
    s=35,
    color="tab:red",
    zorder=3,
)

plt.xscale("log")
plt.xlim(max(1e-2, min(times)*0.8), args.timelimit)

plt.gca().invert_yaxis()

plt.grid(True, which="both", linestyle="--", alpha=0.4)

plt.xlabel("Time (s)")
plt.ylabel("Circuit depth")
plt.title("Evolution of the best solution")

plt.legend()

plt.tight_layout()
plt.savefig(args.output)
plt.show()
