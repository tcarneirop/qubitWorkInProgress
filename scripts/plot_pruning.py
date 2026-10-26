#!/usr/bin/env python3

import os
import sys
import csv
import statistics

import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


# ============================================================
# Arguments
# ============================================================

if len(sys.argv) != 2:
    print(
        "Usage:\n"
        "  ./scripts/plot_pruning.py path/to/kchanges_results.csv"
    )
    sys.exit(1)

csv_file = os.path.abspath(sys.argv[1])

if not os.path.isfile(csv_file):
    print(f"ERROR: CSV not found: {csv_file}")
    sys.exit(1)


# PDF goes next to the CSV
output_pdf = os.path.join(
    os.path.dirname(csv_file),
    "kchanges_comparison.pdf"
)


# ============================================================
# Read CSV
# ============================================================

results = []

with open(csv_file, newline="") as f:

    reader = csv.DictReader(f)

    for row in reader:

        results.append({
            "instance": row["instance"],
            "run": int(row["run"]),
            "gates": int(row["gates"]),
            "sabre_runs": int(row["sabre_runs"]),
            "physic": int(row["physic"]),
            "logic": int(row["logic"]),
            "kchanges": float(row["kchanges"]),
            "pruning": float(row["pruning"]),
        })


if not results:
    print("ERROR: CSV is empty.")
    sys.exit(1)


# ============================================================
# Calculate statistics
# ============================================================

grouped = {}

for r in results:

    instance = r["instance"]

    if instance not in grouped:
        grouped[instance] = []

    grouped[instance].append(r)


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

    # Normalize each execution independently
    ratios = [
        100.0 * p / k
        for p, k in zip(pruning, kchanges)
    ]

    statistics_results.append({

        "instance": instance,

        "gates": runs[0]["gates"],

        "sabre_runs": runs[0]["sabre_runs"],

        "physic": runs[0]["physic"],

        "logic": runs[0]["logic"],

        "num_executions": len(runs),

        "kchanges_mean": statistics.mean(kchanges),

        "kchanges_std": (
            statistics.stdev(kchanges)
            if len(kchanges) > 1
            else 0.0
        ),

        "pruning_mean": statistics.mean(pruning),

        "pruning_std": (
            statistics.stdev(pruning)
            if len(pruning) > 1
            else 0.0
        ),

        "ratio_mean": statistics.mean(ratios),

        "ratio_std": (
            statistics.stdev(ratios)
            if len(ratios) > 1
            else 0.0
        ),
    })


# ============================================================
# PDF
# ============================================================

INSTANCES_PER_PAGE = 10

with PdfPages(output_pdf) as pdf:

    for page_start in range(
        0,
        len(statistics_results),
        INSTANCES_PER_PAGE
    ):

        page_results = statistics_results[
            page_start:
            page_start + INSTANCES_PER_PAGE
        ]

        n = len(page_results)

        fig = plt.figure(
            figsize=(16, 10)
        )

        # ====================================================
        # Graph
        # ====================================================

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
            [
                i - width / 2
                for i in x
            ],
            kchanges_values,
            width,
            label="K-Changes"
        )

        bars_p = ax.bar(
            [
                i + width / 2
                for i in x
            ],
            pruning_values,
            width,
            yerr=pruning_errors,
            capsize=4,
            label="Pruning K-Changes"
        )

        # ----------------------------------------------------
        # Labels above bars
        # ----------------------------------------------------

        for bar in bars_k:

            ax.text(
                bar.get_x()
                + bar.get_width() / 2,
                102,
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
                bar.get_x()
                + bar.get_width() / 2,
                mean + std + 3,
                f"{mean:.1f} ± {std:.1f}%",
                ha="center",
                va="bottom",
                fontsize=8
            )

        # ----------------------------------------------------
        # X axis: ONLY (a), (b), (c)...
        # ----------------------------------------------------

        xlabels = []

        for i in range(n):

            xlabels.append(str(page_start + i + 1))

        ax.set_xticks(x)

        ax.set_xticklabels(
            xlabels,
            fontsize=10
        )

        ax.set_ylabel(
            "Execution time (% of K-Changes)",
            fontsize=11
        )

        ax.set_title(
            "K-Changes vs Pruning K-Changes",
            fontsize=14
        )

        highest = max(
            [100.0]
            + [
                mean + std
                for mean, std in zip(
                    pruning_values,
                    pruning_errors
                )
            ]
        )

        ax.set_ylim(
            0,
            max(110, highest + 15)
        )

        ax.grid(
            axis="y",
            alpha=0.3
        )

        ax.legend()

        # ====================================================
        # Table
        # ====================================================

        table_ax = fig.add_axes(
            [0.04, 0.07, 0.92, 0.30]
        )

        table_ax.axis("off")

        table_data = []

        for i, r in enumerate(page_results):

    

            table_data.append([

                page_start + i + 1,

                r["instance"],

                f"{r['gates']}",

                f"{r['sabre_runs']}",

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
            "SABRE",
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

        table.scale(
            1,
            1.7
        )

        # ====================================================
        # Footer
        # ====================================================

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


print("=" * 60)
print("PDF generated successfully")
print("=" * 60)
print(f"CSV: {csv_file}")
print(f"PDF: {output_pdf}")
