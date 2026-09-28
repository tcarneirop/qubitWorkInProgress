#!/usr/bin/env python3

import os
import glob
import re
import csv
import subprocess
import statistics
import argparse
from datetime import datetime

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.lines import Line2D
from matplotlib.ticker import ScalarFormatter


# ============================================================
# Command-line arguments
# ============================================================

parser = argparse.ArgumentParser(
    description="Benchmark K-Changes vs Pruning K-Changes"
)

parser.add_argument(
    "--sabre-runs",
    type=int,
    default=1,
    help="Number of SABRE runs"
)

parser.add_argument(
    "--num-rand-sols",
    type=int,
    default=1,
    help="Number of random solutions"
)

parser.add_argument(
    "--num-rand-sols-chosen",
    type=int,
    default=1,
    help="Number of random solutions chosen"
)

parser.add_argument(
    "--executions",
    type=int,
    default=10,
    help="Number of independent executions per QASM"
)

parser.add_argument(
    "--plot-only",
    type=str,
    default=None,
    help="Generate plots from an existing CSV instead of running benchmarks"
)

parser.add_argument(
    "--optimization",
    choices=["depth", "gates"],
    required=True,
    help="Optimization target: depth or gates"
)

args = parser.parse_args()


# ============================================================
# Configuration
# ============================================================

QASM_DIR = os.path.expanduser(
    "~/qubitWorkInProgress/qubikos/"
    #"~/qubitWorkInProgress/NEW_Bechmark"
)

# Convention used in this project:
#   depth_qubit.exe -> optimize DEPTH
#   gates_qubit.exe -> optimize GATES
if args.optimization == "depth":
    EXECUTABLE = os.path.expanduser(
        "~/qubitWorkInProgress/depth_qubit.exe"
    )
else:
    EXECUTABLE = os.path.expanduser(
        "~/qubitWorkInProgress/gates_qubit.exe"
    )

OPTIMIZATION = args.optimization
OPTIMIZATION_LABEL = OPTIMIZATION.capitalize()

TOPOLOGY = "albatroz"
SEARCH = "t"

SABRE_RUNS = args.sabre_runs
NUM_RAND_SOLS = args.num_rand_sols
NUM_RAND_SOLS_CHOSEN = args.num_rand_sols_chosen
NUM_EXECUTIONS = args.executions

# Large-instance analysis range.
MIN_LARGE_SWAPS = 100
MAX_LARGE_SWAPS = 10000


# ============================================================
# Experiment directory
# ============================================================

timestamp = datetime.now().strftime(
    "%Y%m%d_%H%M%S"
)

EXPERIMENT_NAME = (
    f"{OPTIMIZATION}_"
    f"sabre{SABRE_RUNS}"
    f"_rand{NUM_RAND_SOLS}"
    f"_chosen{NUM_RAND_SOLS_CHOSEN}"
    f"_exec{NUM_EXECUTIONS}"
    f"_topo-{TOPOLOGY}"
    f"_search-{SEARCH}"
    f"_{timestamp}"
)

BASE_DIR = os.path.dirname(
    os.path.dirname(
        os.path.abspath(__file__)
    )
)

EXPERIMENT_DIR = os.path.join(
    BASE_DIR,
    "prining_exec",
    EXPERIMENT_NAME
)

LOG_DIR = os.path.join(
    EXPERIMENT_DIR,
    "logs"
)

OUTPUT_PDF = os.path.join(
    EXPERIMENT_DIR,
    f"{OPTIMIZATION}_kchanges_comparison.pdf"
)

OUTPUT_SABRE_PDF = os.path.join(
    EXPERIMENT_DIR,
    f"{OPTIMIZATION}_sabre_time_comparison.pdf"
)

OUTPUT_SWAPS_PDF = os.path.join(
    EXPERIMENT_DIR,
    f"{OPTIMIZATION}_pruning_vs_swaps.pdf"
)

OUTPUT_CSV = os.path.join(
    EXPERIMENT_DIR,
    f"{OPTIMIZATION}_results.csv"
)

os.makedirs(
    LOG_DIR,
    exist_ok=True
)


# Maximum number of instances per PDF page
INSTANCES_PER_PAGE = 10


# ============================================================
# Run one QASM once
# ============================================================

def run_qasm(qasmfile, run_number):

    instance = os.path.basename(qasmfile)

    command = [
        EXECUTABLE,
        "--search", SEARCH,
        "--sabre-runs", str(SABRE_RUNS),
        "--topology", TOPOLOGY,
        "--qasmfile", qasmfile,
        "--num-rand-sols", str(NUM_RAND_SOLS),
        "--num-rand-sols-chosen", str(NUM_RAND_SOLS_CHOSEN),
        "--pruning",
    ]

    print("\n" + "=" * 70)
    print(
        f"Running: {instance} "
        f"[execution {run_number}/{NUM_EXECUTIONS}]"
    )
    print("=" * 70)

    result = subprocess.run(
        command,
        capture_output=True,
        text=True
    )

    output = result.stdout
    error_output = result.stderr

    # --------------------------------------------------------
    # Save complete log, including stderr
    # --------------------------------------------------------

    logfile = os.path.join(
        LOG_DIR,
        instance.replace(
            ".qasm",
            f"_run{run_number:02d}.log"
        )
    )

    with open(logfile, "w") as f:

        f.write(output)

        if error_output:
            f.write(
                "\n\n################ STDERR ################\n\n"
            )
            f.write(error_output)

        f.write(
            "\n\n################ COMMAND ################\n\n"
        )
        f.write(
            " ".join(command)
        )
        f.write("\n")

    # --------------------------------------------------------
    # Check executable return code
    # --------------------------------------------------------

    if result.returncode != 0:

        raise RuntimeError(
            f"gates_qubit.exe returned "
            f"code {result.returncode}. "
            f"See {logfile}"
        )

    # --------------------------------------------------------
    # Extract circuit information
    # --------------------------------------------------------

    gates_match = re.search(
        r"circuit_flat\.num_gates:\s*(\d+)",
        output
    )

    # --------------------------------------------------------
    # Extract the final REPORT sections.
    # The executable reports:
    #   K-CHANGES
    #   PRUNING K-CHANGES
    # --------------------------------------------------------

    report_match = re.search(
        r"########################## REPORT ##########################(.*)\Z",
        output,
        re.DOTALL,
    )

    if report_match is None:
        raise RuntimeError(
            f"Could not find final REPORT. See {logfile}"
        )

    report = report_match.group(1)

    k_match = re.search(
        r"(?m)^[ \t]*K-CHANGES[ \t]*$",
        report,
    )

    pruning_match = re.search(
        r"(?m)^[ \t]*PRUNING K-CHANGES[ \t]*$",
        report,
    )

    if k_match is None or pruning_match is None:
        raise RuntimeError(
            f"Could not isolate K-CHANGES and PRUNING K-CHANGES sections. "
            f"See {logfile}"
        )

    k_section = report[k_match.end():pruning_match.start()]
    pruning_section = report[pruning_match.end():]

    k_sabre_match = re.search(
        r"Number of SABRE runs:\s*(\d+)",
        k_section
    )
    pruning_sabre_match = re.search(
        r"Number of SABRE runs:\s*(\d+)",
        pruning_section
    )

    qubits_match = re.search(
        r"Physic QUBITS:\s*(\d+)\s+Logic QUBITS:\s*(\d+)",
        output
    )

    # --------------------------------------------------------
    # Extract elapsed times
    #
    # First  = K-Changes
    # Second = Pruning K-Changes
    # --------------------------------------------------------

    k_times = re.findall(
        r"Elapsed k-changes:\s*([0-9.eE+-]+)",
        k_section
    )

    pruning_times = re.findall(
        r"Elapsed k-changes:\s*([0-9.eE+-]+)",
        pruning_section
    )

    if not k_times or not pruning_times:

        raise RuntimeError(
            "Could not find both elapsed k-changes times. "
            f"See {logfile}"
        )

    kchanges_time = float(k_times[0])
    pruning_time = float(pruning_times[0])

    # --------------------------------------------------------
    # Validate extracted information
    # --------------------------------------------------------

    if not gates_match:
        raise RuntimeError(
            f"Could not find number of gates. See {logfile}"
        )

    if not k_sabre_match or not pruning_sabre_match:
        raise RuntimeError(
            f"Could not find section-specific SABRE run counts. See {logfile}"
        )

    if not qubits_match:
        raise RuntimeError(
            f"Could not find qubit information. See {logfile}"
        )

    gates = int(gates_match.group(1))

    kchanges_sabre_runs = int(
        k_sabre_match.group(1)
    )

    pruning_sabre_runs = int(
        pruning_sabre_match.group(1)
    )

    physic_qubits = int(
        qubits_match.group(1)
    )

    logic_qubits = int(
        qubits_match.group(2)
    )

    # --------------------------------------------------------
    # Normalize THIS execution
    # --------------------------------------------------------

    pruning_percent = (
        100.0
        * pruning_time
        / kchanges_time
    )

    data = {
        "instance": instance,
        "run": run_number,
        "gates": gates,
        "sabre_runs": kchanges_sabre_runs,
        "kchanges_sabre_runs": kchanges_sabre_runs,
        "pruning_sabre_runs": pruning_sabre_runs,
        "physic": physic_qubits,
        "logic": logic_qubits,
        "kchanges": kchanges_time,
        "pruning": pruning_time,
        "kchanges_per_sabre": kchanges_time / kchanges_sabre_runs,
        "pruning_per_sabre": pruning_time / pruning_sabre_runs,
        "pruning_percent": pruning_percent,
        "logfile": logfile,
    }

    print(f"Gates:              {gates}")
    print(f"K-Changes SABRE:    {kchanges_sabre_runs}")
    print(f"Pruning SABRE:      {pruning_sabre_runs}")
    print(f"Physical qubits:    {physic_qubits}")
    print(f"Logical qubits:     {logic_qubits}")
    print(f"K-Changes:          {kchanges_time:.6f} s")
    print(f"Pruning K-Changes:  {pruning_time:.6f} s")
    print(f"Pruning normalized: {pruning_percent:.2f}%")
    print(f"Log:                {logfile}")

    return data


# ============================================================
# Load CSV for --plot-only
# ============================================================

def load_csv(filename):

    results = []

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)

        if reader.fieldnames is None:
            raise RuntimeError(f"CSV has no header: {filename}")

        fields = set(reader.fieldnames)

        # ----------------------------------------------------
        # New CSV format used by the depth/gates benchmark:
        #   instance, metric, run, swaps_input, twoq, oneq,
        #   kchanges_depth, kchanges_gates, kchanges_swaps,
        #   kchanges_time, pruning_depth, pruning_gates,
        #   pruning_swaps, pruning_time, time_ratio_percent,
        #   logfile
        # ----------------------------------------------------
        new_format = {
            "instance",
            "metric",
            "run",
            "swaps_input",
            "twoq",
            "oneq",
            "kchanges_time",
            "pruning_time",
        }.issubset(fields)

        # ----------------------------------------------------
        # Legacy format produced by this script:
        #   instance, run, gates, sabre_runs, ...
        # ----------------------------------------------------
        legacy_format = {
            "instance",
            "run",
            "gates",
            "kchanges",
            "pruning",
        }.issubset(fields)

        if not new_format and not legacy_format:
            raise RuntimeError(
                "Unsupported CSV format. Expected either the current "
                "depth/gates CSV schema or the legacy plotting schema."
            )

        for row in reader:

            if new_format:

                # A combined CSV can contain both depth and gates.
                # Plot only the requested optimization.
                metric = row.get("metric", "").strip().lower()
                if metric and metric != OPTIMIZATION:
                    continue

                instance = row["instance"]
                run = int(row["run"])

                kchanges = float(row["kchanges_time"])
                pruning = float(row["pruning_time"])

                if "time_ratio_percent" in fields and row["time_ratio_percent"]:
                    pruning_percent = float(row["time_ratio_percent"])
                else:
                    pruning_percent = 100.0 * pruning / kchanges

                twoq = int(row["twoq"])
                oneq = int(row["oneq"])
                total_gates = twoq + oneq

                results.append({
                    "instance": instance,
                    "run": run,
                    "gates": total_gates,
                    "sabre_runs": 1,
                    "kchanges_sabre_runs": 1,
                    "pruning_sabre_runs": 1,
                    "physic": int(row["swaps_input"]),
                    "logic": int(row["kchanges_depth"]),
                    "kchanges": kchanges,
                    "pruning": pruning,
                    "kchanges_per_sabre": kchanges,
                    "pruning_per_sabre": pruning,
                    "pruning_percent": pruning_percent,
                    "logfile": row.get("logfile", ""),
                })

            else:

                # Legacy CSV has only one optimization per file.
                kchanges = float(row["kchanges"])
                pruning = float(row["pruning"])

                if "pruning_percent" in fields and row["pruning_percent"]:
                    pruning_percent = float(row["pruning_percent"])
                else:
                    pruning_percent = 100.0 * pruning / kchanges

                results.append({
                    "instance": row["instance"],
                    "run": int(row["run"]),
                    "gates": int(row["gates"]),
                    "sabre_runs": int(row.get("sabre_runs", 1)),
                    "kchanges_sabre_runs": int(
                        row.get("kchanges_sabre_runs", 1)
                    ),
                    "pruning_sabre_runs": int(
                        row.get("pruning_sabre_runs", 1)
                    ),
                    "physic": int(row.get("physic", 0)),
                    "logic": int(row.get("logic", 0)),
                    "kchanges": kchanges,
                    "pruning": pruning,
                    "kchanges_per_sabre": float(
                        row.get("kchanges_per_sabre", kchanges)
                    ),
                    "pruning_per_sabre": float(
                        row.get("pruning_per_sabre", pruning)
                    ),
                    "pruning_percent": pruning_percent,
                    "logfile": row.get("logfile", ""),
                })

    if not results:
        raise RuntimeError(
            f"No rows for optimization '{OPTIMIZATION}' were found in {filename}."
        )

    return results


# ============================================================
# Statistics
# ============================================================

def calculate_statistics(results):

    grouped = {}

    for result in results:

        instance = result["instance"]

        if instance not in grouped:
            grouped[instance] = []

        grouped[instance].append(result)

    statistics_results = []

    for instance, runs in grouped.items():

        kchanges = [
            r["kchanges"]
            for r in runs
        ]

        pruning = [
            r["pruning"]
            for r in runs
        ]

        ratios = [
            r["pruning_percent"]
            for r in runs
        ]

        k_per_sabre = [
            r["kchanges_per_sabre"]
            for r in runs
        ]

        p_per_sabre = [
            r["pruning_per_sabre"]
            for r in runs
        ]

        statistics_results.append({

            "instance": instance,

            "gates": runs[0]["gates"],

            "sabre_runs": runs[0]["sabre_runs"],
            "kchanges_sabre_runs": runs[0]["kchanges_sabre_runs"],
            "pruning_sabre_runs": runs[0]["pruning_sabre_runs"],

            "physic": runs[0]["physic"],

            "logic": runs[0]["logic"],

            "num_executions": len(runs),

            # Absolute K-Changes time
            "kchanges_mean": statistics.mean(kchanges),

            "kchanges_std": (
                statistics.stdev(kchanges)
                if len(kchanges) > 1
                else 0.0
            ),

            # Absolute Pruning time
            "pruning_mean": statistics.mean(pruning),

            "pruning_std": (
                statistics.stdev(pruning)
                if len(pruning) > 1
                else 0.0
            ),

            # Per-run normalized ratio
            "ratio_mean": statistics.mean(ratios),

            "ratio_std": (
                statistics.stdev(ratios)
                if len(ratios) > 1
                else 0.0
            ),

            "kchanges_per_sabre_mean": statistics.mean(k_per_sabre),
            "kchanges_per_sabre_std": (
                statistics.stdev(k_per_sabre)
                if len(k_per_sabre) > 1
                else 0.0
            ),

            "pruning_per_sabre_mean": statistics.mean(p_per_sabre),
            "pruning_per_sabre_std": (
                statistics.stdev(p_per_sabre)
                if len(p_per_sabre) > 1
                else 0.0
            ),
        })

    statistics_results.sort(
        key=lambda r: tuple(
            int(x)
            for x in os.path.splitext(r["instance"])[0].split("_")[:3]
        )
    )

    return statistics_results


# ============================================================
# Save raw CSV
# ============================================================

def save_csv(results):

    fieldnames = [
        "instance",
        "run",
        "gates",
        "sabre_runs",
        "kchanges_sabre_runs",
        "pruning_sabre_runs",
        "physic",
        "logic",
        "kchanges",
        "pruning",
        "kchanges_per_sabre",
        "pruning_per_sabre",
        "pruning_percent",
        "logfile",
    ]

    with open(
        OUTPUT_CSV,
        "w",
        newline=""
    ) as f:

        writer = csv.DictWriter(
            f,
            fieldnames=fieldnames
        )

        writer.writeheader()
        writer.writerows(results)


# ============================================================
# Create PDF
# ============================================================

def create_pdf(statistics_results):

    # One page per number of swaps. Inside each page, instances are
    # ordered by number of two-qubit gates and then one-qubit gates.
    swap_groups = {}

    for r in statistics_results:
        parts = os.path.splitext(r["instance"])[0].split("_")
        if len(parts) < 3:
            continue

        try:
            swaps = int(parts[0])
            twoq = int(parts[1])
            oneq = int(parts[2])
        except ValueError:
            continue

        r2 = dict(r)
        r2["swaps"] = swaps
        r2["twoq"] = twoq
        r2["oneq"] = oneq
        swap_groups.setdefault(swaps, []).append(r2)

    with PdfPages(OUTPUT_PDF) as pdf:

        for swaps in sorted(swap_groups):

            page_results = sorted(
                swap_groups[swaps],
                key=lambda r: (r["twoq"], r["oneq"])
            )

            n = len(page_results)

            fig = plt.figure(
                figsize=(16, 10)
            )

            # =================================================
            # Graph
            # =================================================

            ax = fig.add_axes(
                [0.08, 0.46, 0.88, 0.43]
            )

            x = list(range(n))

            kchanges_values = [
                100.0
                for _ in page_results
            ]

            pruning_values = [
                r["ratio_mean"]
                for r in page_results
            ]

            pruning_errors = [
                r["ratio_std"]
                for r in page_results
            ]

            width = 0.36

            bars_k = ax.bar(
                [i - width / 2 for i in x],
                kchanges_values,
                width,
                label="K-Changes"
            )

            bars_p = ax.bar(
                [i + width / 2 for i in x],
                pruning_values,
                width,
                yerr=pruning_errors,
                capsize=4,
                label="Pruning K-Changes"
            )

            for bar in bars_k:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + 2,
                    "100%",
                    ha="center",
                    va="bottom",
                    fontsize=9
                )

            for bar, mean, std in zip(
                bars_p,
                pruning_values,
                pruning_errors
            ):
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    mean + std + 3,
                    f"{mean:.1f} ± {std:.1f}%",
                    ha="center",
                    va="bottom",
                    fontsize=8
                )

            ax.set_xticks(x)
            ax.set_xticklabels(
                [str(i + 1) for i in x],
                fontsize=9
            )

            ax.set_xlabel(
                "Instance ID (ordered by # two-qubit gates)",
                fontsize=11
            )

            ax.set_ylabel(
                "Execution time (% of K-Changes)",
                fontsize=11
            )

            ax.set_title(
                f"{OPTIMIZATION_LABEL} optimization — "
                f"K-Changes vs Pruning K-Changes\n"
                f"# swaps = {swaps}",
                fontsize=14
            )

            ax.set_ylim(0, 100)

            ax.grid(
                axis="y",
                alpha=0.3
            )

            ax.legend()

            # =================================================
            # Table
            # =================================================

            table_ax = fig.add_axes(
                [0.04, 0.07, 0.92, 0.30]
            )

            table_ax.axis("off")

            table_data = []

            for i, r in enumerate(page_results):
                table_data.append([
                    f"{i + 1}",
                    r["instance"],
                    f"{r['gates']}",
                    f"{r['kchanges_sabre_runs']} / {r['pruning_sabre_runs']}",
                    f"{r['physic']}",
                    f"{r['logic']}",
                    (
                        f"{r['kchanges_mean']:.4f} "
                        f"± {r['kchanges_std']:.4f}"
                    ),
                    (
                        f"{r['pruning_mean']:.4f} "
                        f"± {r['pruning_std']:.4f}"
                    ),
                    (
                        f"{r['ratio_mean']:.2f} "
                        f"± {r['ratio_std']:.2f}%"
                    ),
                ])

            columns = [
                "ID",
                "Instance",
                "Gates",
                "K SABRE / P SABRE",
                "Phys",
                "Logic",
                "K-Changes (s)",
                "Pruning (s)",
                "Pruning / K (%)",
            ]

            table = table_ax.table(
                cellText=table_data,
                colLabels=columns,
                cellLoc="center",
                loc="center",
                colWidths=[
                    0.045,
                    0.24,
                    0.065,
                    0.065,
                    0.065,
                    0.065,
                    0.14,
                    0.14,
                    0.14,
                ],
            )

            table.auto_set_font_size(False)
            table.set_fontsize(8.5)
            table.scale(1, 1.7)

            fig.text(
                0.5,
                0.025,
                "Pruning / K (%) = mean of "
                "[Pruning time / K-Changes time × 100] "
                "over independent executions; "
                "error bars show ±1 standard deviation.",
                ha="center",
                fontsize=8.5
            )

            pdf.savefig(
                fig,
                bbox_inches="tight"
            )

            plt.close(fig)


# ============================================================
# Create SABRE-time PDF
# ============================================================

def create_sabre_pdf(statistics_results):

    # One page per number of swaps. Inside each page, instances are
    # ordered by number of two-qubit gates and then one-qubit gates.
    swap_groups = {}

    for r in statistics_results:
        parts = os.path.splitext(r["instance"])[0].split("_")
        if len(parts) < 3:
            continue

        try:
            swaps = int(parts[0])
            twoq = int(parts[1])
            oneq = int(parts[2])
        except ValueError:
            continue

        r2 = dict(r)
        r2["swaps"] = swaps
        r2["twoq"] = twoq
        r2["oneq"] = oneq
        swap_groups.setdefault(swaps, []).append(r2)

    with PdfPages(OUTPUT_SABRE_PDF) as pdf:

        for swaps in sorted(swap_groups):

            page_results = sorted(
                swap_groups[swaps],
                key=lambda r: (r["twoq"], r["oneq"])
            )

            n = len(page_results)

            fig = plt.figure(
                figsize=(16, 10)
            )

            # =================================================
            # Graph
            # =================================================

            ax = fig.add_axes(
                [0.08, 0.46, 0.88, 0.43]
            )

            x = list(range(n))
            width = 0.36

            kvalues = [
                r["kchanges_per_sabre_mean"]
                for r in page_results
            ]

            pvalues = [
                r["pruning_per_sabre_mean"]
                for r in page_results
            ]

            kerrors = [
                r["kchanges_per_sabre_std"]
                for r in page_results
            ]

            perrors = [
                r["pruning_per_sabre_std"]
                for r in page_results
            ]

            bars_k = ax.bar(
                [i - width / 2 for i in x],
                kvalues,
                width,
                yerr=kerrors,
                capsize=4,
                label="K-Changes"
            )

            bars_p = ax.bar(
                [i + width / 2 for i in x],
                pvalues,
                width,
                yerr=perrors,
                capsize=4,
                label="Pruning K-Changes"
            )

            for bar, mean, std in zip(
                bars_k, kvalues, kerrors
            ):
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    mean + std,
                    f"{mean:.5f} ± {std:.5f}",
                    ha="center",
                    va="bottom",
                    fontsize=8
                )

            for bar, mean, std in zip(
                bars_p, pvalues, perrors
            ):
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    mean + std,
                    f"{mean:.5f} ± {std:.5f}",
                    ha="center",
                    va="bottom",
                    fontsize=8
                )

            ax.set_xticks(x)
            ax.set_xticklabels(
                [str(i + 1) for i in x],
                fontsize=9
            )

            ax.set_xlabel(
                "Instance ID (ordered by # two-qubit gates)",
                fontsize=11
            )

            ax.set_ylabel(
                "Execution time per SABRE run (s)",
                fontsize=11
            )

            ax.set_title(
                f"{OPTIMIZATION_LABEL} optimization — K-Changes vs Pruning K-Changes: Time per SABRE Run\n"
                f"# swaps = {swaps}",
                fontsize=14
            )

            highest = max(
                [
                    mean + std
                    for mean, std in zip(kvalues, kerrors)
                ] + [
                    mean + std
                    for mean, std in zip(pvalues, perrors)
                ]
            )

            ax.set_ylim(
                0,
                max(highest * 1.25, 0.001)
            )

            ax.grid(
                axis="y",
                alpha=0.3
            )

            ax.legend()

            # =================================================
            # Table
            # =================================================

            table_ax = fig.add_axes(
                [0.04, 0.07, 0.92, 0.30]
            )

            table_ax.axis("off")

            table_data = []

            for i, r in enumerate(page_results):

                table_data.append([
                    i + 1,
                    r["instance"],
                    f"{r['gates']}",
                    f"{r['kchanges_sabre_runs']}",
                    f"{r['pruning_sabre_runs']}",
                    f"{r['physic']}",
                    f"{r['logic']}",
                    (
                        f"{r['kchanges_per_sabre_mean']:.5f} "
                        f"± {r['kchanges_per_sabre_std']:.5f}"
                    ),
                    (
                        f"{r['pruning_per_sabre_mean']:.5f} "
                        f"± {r['pruning_per_sabre_std']:.5f}"
                    ),
                ])

            columns = [
                "ID",
                "Instance",
                "Gates",
                "K SABRE",
                "P SABRE",
                "Phys",
                "Logic",
                "K-Changes / SABRE (s)",
                "Pruning / SABRE (s)",
            ]

            table = table_ax.table(
                cellText=table_data,
                colLabels=columns,
                cellLoc="center",
                loc="center",
                colWidths=[
                    0.05,
                    0.18,
                    0.07,
                    0.07,
                    0.07,
                    0.07,
                    0.07,
                    0.17,
                    0.17,
                ],
            )

            table.auto_set_font_size(False)
            table.set_fontsize(8)
            table.scale(1, 1.55)

            for cell in table.get_celld().values():
                cell.set_linewidth(0.7)

            fig.tight_layout()
            pdf.savefig(fig)
            plt.close(fig)


def create_swaps_pdf(statistics_results):

    rows = []

    for r in statistics_results:

        name = os.path.splitext(
            os.path.basename(r["instance"])
        )[0]

        parts = name.split("_")

        if len(parts) < 3:
            continue

        try:
            swaps = int(parts[0])
            twoq = int(parts[1])
            oneq = int(parts[2])
        except ValueError:
            continue

        rows.append({
            "swaps": swaps,
            "twoq": twoq,
            "oneq": oneq,
            "ratio": r["ratio_mean"],
        })

    if not rows:
        return

    with PdfPages(
        os.path.join(
            EXPERIMENT_DIR,
            f"{OPTIMIZATION}_pruning_vs_swaps.pdf"
        )
    ) as pdf:

        fig, ax = plt.subplots(figsize=(12, 7))

        x = [r["swaps"] for r in rows]
        y = [r["ratio"] for r in rows]

        # Color-code points by total number of gates so that points with the
        # same circuit size can be tracked across different # swaps values.
        gate_values = sorted(set(r["twoq"] + r["oneq"] for r in rows))
        cmap = plt.get_cmap("tab10")
        gate_colors = {
            gates: cmap(i % cmap.N)
            for i, gates in enumerate(gate_values)
        }

        point_colors = [
            gate_colors[r["twoq"] + r["oneq"]]
            for r in rows
        ]

        ax.axhline(
            100,
            linestyle="--",
            linewidth=1.5
        )

        ax.scatter(
            x,
            y,
            s=42,
            alpha=0.75,
            c=point_colors
        )

        legend_handles = [
            Line2D(
                [0],
                [0],
                marker="o",
                linestyle="none",
                markerfacecolor=gate_colors[gates],
                markeredgecolor=gate_colors[gates],
                markersize=7,
                label=f"{gates} gates"
            )
            for gates in gate_values
        ]

        ax.legend(
            handles=legend_handles,
            title="# gates",
            loc="upper left"
        )

        ax.set_xscale("log")

        # Keep the logarithmic spacing, but display tick labels normally
        # (e.g. 100, 1000, 10000 instead of 10², 10³, 10⁴).
        plain_formatter = ScalarFormatter()
        plain_formatter.set_scientific(False)
        plain_formatter.set_useOffset(False)
        ax.xaxis.set_major_formatter(plain_formatter)

        ax.set_xlabel("# swaps")
        ax.set_ylabel("Pruning / K-Changes (%)")
        ax.set_ylim(0, 100)

        ax.set_title(
            f"{OPTIMIZATION_LABEL} optimization — Pruning Advantage as a Function of the Number of Swaps"
        )

        grouped = {}

        for r in rows:

            s = r["swaps"]

            if s not in grouped:
                grouped[s] = {
                    "pruning": 0,
                    "total": 0
                }

            grouped[s]["total"] += 1

            if r["ratio"] < 100:
                grouped[s]["pruning"] += 1

        for s, counts in sorted(grouped.items()):

            ax.annotate(
                f"{counts['pruning']}/{counts['total']}",
                xy=(s, 100),
                xytext=(0, -25),
                textcoords="offset points",
                ha="center",
                va="top",
                fontsize=10
            )

        ax.text(
            0.98,
            0.96,
            "Below 100% → Pruning faster\n"
            "Above 100% → K-Changes faster",
            transform=ax.transAxes,
            ha="right",
            va="top",
            fontsize=10
        )

        ax.grid(alpha=0.25)

        pdf.savefig(
            fig,
            bbox_inches="tight"
        )

        plt.close(fig)


# ============================================================
# Pruning advantage as a function of number of gates
# for each fixed number of swaps
# ============================================================

def create_gates_vs_swaps_pdf(statistics_results):

    rows = []

    for r in statistics_results:
        name = os.path.splitext(
            os.path.basename(r["instance"])
        )[0]

        parts = name.split("_")

        if len(parts) < 3:
            continue

        try:
            swaps = int(parts[0])
            twoq = int(parts[1])
            oneq = int(parts[2])
        except ValueError:
            continue

        total_gates = twoq + oneq

        if total_gates <= 0:
            continue

        ratio = r["ratio_mean"]

        if not np.isfinite(ratio):
            continue

        rows.append({
            "swaps": swaps,
            "gates": total_gates,
            "ratio": ratio,
        })

    if not rows:
        return

    grouped = {}

    for r in rows:
        grouped.setdefault(r["swaps"], []).append(r)

    for swaps in grouped:
        grouped[swaps].sort(key=lambda r: r["gates"])

    output = os.path.join(
        EXPERIMENT_DIR,
        f"{OPTIMIZATION}_pruning_advantage_vs_gates.pdf"
    )

    with PdfPages(output) as pdf:

        for swaps in sorted(grouped):

            group = grouped[swaps]

            fig, ax = plt.subplots(figsize=(12, 7))

            ax.scatter(
                [r["gates"] for r in group],
                [r["ratio"] for r in group],
                s=60,
                alpha=0.85
            )

            ax.axhline(
                100,
                linestyle="--",
                linewidth=1.2
            )

            ax.set_xscale("log")

            # Keep logarithmic spacing, but display tick labels normally
            # (e.g. 100, 1000, 10000 instead of 10², 10³, 10⁴).
            plain_formatter = ScalarFormatter()
            plain_formatter.set_scientific(False)
            plain_formatter.set_useOffset(False)
            ax.xaxis.set_major_formatter(plain_formatter)

            ax.set_xlabel("# two-qubit gates")
            ax.set_ylabel("Pruning / K-Changes (%)")
            ax.set_ylim(0, 100)

            ax.set_title(
                f"{OPTIMIZATION_LABEL} optimization — "
                f"Pruning Advantage vs # two-qubit gates\n"
                f"# swaps = {swaps}"
            )

            ax.text(
                0.98,
                0.96,
                "Below 100% → Pruning faster\n"
                "Above 100% → K-Changes faster",
                transform=ax.transAxes,
                ha="right",
                va="top",
                fontsize=10
            )

            ax.grid(alpha=0.25)

            fig.tight_layout()

            pdf.savefig(
                fig,
                bbox_inches="tight"
            )

            plt.close(fig)


# ============================================================
# Large-instance comparison PDF
# ============================================================

def create_large_swaps_pdf(statistics_results):

    rows = []

    for r in statistics_results:
        name = os.path.splitext(
            os.path.basename(r["instance"])
        )[0]

        parts = name.split("_")

        if len(parts) < 3:
            continue

        try:
            swaps = int(parts[0])
            twoq = int(parts[1])
            oneq = int(parts[2])
        except ValueError:
            continue

        total_gates = twoq + oneq

        if total_gates <= 0:
            continue

        ratio = r["ratio_mean"]
        if not np.isfinite(ratio):
            continue

        rows.append({
            "swaps": swaps,
            "twoq": twoq,
            "ratio": ratio,
            "density": swaps / total_gates,
        })

    if not rows:
        return

    density = np.array(
        [r["density"] for r in rows],
        dtype=float
    )

    ratios = np.array(
        [r["ratio"] for r in rows],
        dtype=float
    )

    twoq_gates = np.array(
        [r["twoq"] for r in rows],
        dtype=float
    )

    # Correlation is undefined when density has zero variance.
    density_corr = (
        np.corrcoef(density, ratios)[0, 1]
        if len(rows) > 1 and np.std(density) > 0 and np.std(ratios) > 0
        else float("nan")
    )

    ymin = min(ratios.min(), 100.0)
    ymax = max(ratios.max(), 100.0)
    margin = max(5.0, 0.08 * (ymax - ymin))

    output = os.path.join(
        EXPERIMENT_DIR,
        f"{OPTIMIZATION}_pruning_advantage.pdf"
    )

    with PdfPages(output) as pdf:

        fig, ax = plt.subplots(figsize=(10, 6))

        ax.axhline(
            100,
            linestyle="--",
            linewidth=1.2
        )

        unique_gates = sorted({int(r["twoq"]) for r in rows})
        gate_colors = {
            gates: plt.cm.viridis(i / max(1, len(unique_gates) - 1))
            for i, gates in enumerate(unique_gates)
        }
        point_colors = [gate_colors[int(g)] for g in twoq_gates]

        ax.scatter(
            density,
            ratios,
            c=point_colors,
            s=55,
            alpha=0.8
        )

        # Explicit gate-count legend: one colored dot per # two-qubit gates.
        gate_handles = [
            Line2D(
                [0],
                [0],
                marker="o",
                linestyle="none",
                markerfacecolor=gate_colors[gates],
                markeredgecolor=gate_colors[gates],
                markersize=7,
                label=f"{gates} gates"
            )
            for gates in unique_gates
        ]

        ax.set_xlabel("# swaps / total gates")
        ax.set_ylabel("Pruning / K-Changes (%)")

        corr_label = (
            f"r = {density_corr:.3f}"
            if np.isfinite(density_corr)
            else "r = nan"
        )

        ax.set_title(
            f"{OPTIMIZATION_LABEL} optimization — # swaps / total gates\n"
            f"{corr_label}"
        )

        ax.set_ylim(0, 100)

        ax.grid(alpha=0.25)

        fig.suptitle(
            f"Pruning Advantage — optimizing for {OPTIMIZATION_LABEL}",
            fontsize=14
        )

        # Keep both the gate legend and the interpretation note outside the
        # plotting area so neither can cover the data points.
        fig.legend(
            handles=gate_handles,
            title="# two-qubit gates",
            loc="center left",
            bbox_to_anchor=(0.89, 0.50),
            frameon=True
        )

        fig.text(
            0.5,
            0.02,
            "Below 100% → Pruning faster    |    "
            "Above 100% → K-Changes faster",
            ha="center",
            fontsize=9
        )

        fig.tight_layout(
            rect=[0.00, 0.05, 0.87, 0.94]
        )

        pdf.savefig(
            fig,
            bbox_inches="tight"
        )

        plt.close(fig)

        # Summary page.
        fig, ax = plt.subplots(figsize=(10, 4))
        ax.axis("off")

        summary = [
            ["Population", f"{len(rows)} instances"],
            [
                "Swaps/total gates vs Pruning/K",
                corr_label
            ],
        ]

        table = ax.table(
            cellText=summary,
            colLabels=["Measure", "Value"],
            cellLoc="left",
            loc="center",
            colWidths=[0.55, 0.30]
        )

        table.auto_set_font_size(False)
        table.set_fontsize(11)
        table.scale(1, 2)

        fig.suptitle(
            "Large-Instance Correlation Summary",
            fontsize=14
        )

        pdf.savefig(
            fig,
            bbox_inches="tight"
        )

        plt.close(fig)


# ============================================================
# Main
# ============================================================

def main():

    if args.plot_only:

        results = load_csv(
            args.plot_only
        )

        # Plot-only output goes next to the CSV.
        global EXPERIMENT_DIR
        global OUTPUT_PDF
        global OUTPUT_CSV
        global OUTPUT_SABRE_PDF
        global OUTPUT_SWAPS_PDF

        EXPERIMENT_DIR = os.path.dirname(
            os.path.abspath(args.plot_only)
        )

        OUTPUT_CSV = os.path.abspath(
            args.plot_only
        )

        OUTPUT_PDF = os.path.join(
            EXPERIMENT_DIR,
            f"{OPTIMIZATION}_kchanges_comparison.pdf"
        )

        OUTPUT_SABRE_PDF = os.path.join(
            EXPERIMENT_DIR,
            f"{OPTIMIZATION}_sabre_time_comparison.pdf"
        )

        OUTPUT_SWAPS_PDF = os.path.join(
            EXPERIMENT_DIR,
            "pruning_vs_swaps.pdf"
        )

    else:

        qasm_files = sorted(
            glob.glob(
                os.path.join(
                    QASM_DIR,
                    "*.qasm"
                )
            )
        )

        if not qasm_files:
            raise RuntimeError(
                f"No .qasm files found in {QASM_DIR}"
            )

        print("\n" + "=" * 70)
        print("K-CHANGES BENCHMARK")
        print("=" * 70)

        print(f"Optimization:    {OPTIMIZATION_LABEL}")
        print(f"QASM directory:  {QASM_DIR}")
        print(f"Executable:      {EXECUTABLE}")
        print(f"Topology:        {TOPOLOGY}")
        print(f"Search:          {SEARCH}")
        print(f"SABRE runs:      {SABRE_RUNS}")
        print(f"Random sols:     {NUM_RAND_SOLS}")
        print(f"Executions:      {NUM_EXECUTIONS}")
        print(f"Instances:       {len(qasm_files)}")
        print(f"Output directory:{EXPERIMENT_DIR}")

        print("=" * 70)

        results = []

        for qasmfile in qasm_files:

            for run_number in range(
                1,
                NUM_EXECUTIONS + 1
            ):

                try:

                    result = run_qasm(
                        qasmfile,
                        run_number
                    )

                    results.append(result)

                except Exception as e:

                    print(
                        f"\nERROR processing "
                        f"{os.path.basename(qasmfile)} "
                        f"(run {run_number}):"
                    )

                    print(e)

        if not results:
            raise RuntimeError(
                "No successful results."
            )

        save_csv(results)

    statistics_results = calculate_statistics(
        results
    )

    create_pdf(
        statistics_results
    )

    create_sabre_pdf(
        statistics_results
    )

    create_swaps_pdf(
        statistics_results
    )

    create_large_swaps_pdf(
        statistics_results
    )

    create_gates_vs_swaps_pdf(
        statistics_results
    )

    print("\n" + "=" * 70)
    print("DONE")
    print("=" * 70)

    print(f"CSV:              {OUTPUT_CSV}")
    print(f"Main PDF:         {OUTPUT_PDF}")
    print(f"SABRE PDF:        {OUTPUT_SABRE_PDF}")
    print(
        f"Swaps PDF:        "
        f"{os.path.join(EXPERIMENT_DIR, 'pruning_vs_swaps.pdf')}"
    )
    print(
        f"Pruning advantage: {os.path.join(EXPERIMENT_DIR, f'{OPTIMIZATION}_pruning_advantage.pdf')}"
    )
    print(
        f"Pruning vs gates:  {os.path.join(EXPERIMENT_DIR, f'{OPTIMIZATION}_pruning_advantage_vs_gates.pdf')}"
    )


if __name__ == "__main__":
    main()