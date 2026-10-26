import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import subprocess
import os
import re
import sys
from pathlib import Path


EXECUTABLE = "./DEBUG_depth_qubit.exe"
OMP_THREADS = "12"


def parse_log(filename):
    instance = None
    physical_qubits = None
    logical_qubits = None
    optimization = None
    num_complete_sols = None
    depths = []
    swaps = []
    section = None

    with open(filename) as f:
        for line in f:
            line = line.strip()

            if line.startswith("circuit_flat.filename:"):
                qasm = line.split(":", 1)[1].strip()
                instance = Path(qasm).stem

            elif line.startswith("Physic QUBITS:"):
                match = re.search(
                    r"Physic QUBITS:\s*(\d+)\s+Logic QUBITS:\s*(\d+)",
                    line
                )
                if match:
                    physical_qubits = int(match.group(1))
                    logical_qubits = int(match.group(2))

            elif "Optimizing for DEPTH" in line:
                optimization = "DEPTH"

            elif "Optimizing for GATES" in line:
                optimization = "GATES"

            elif line.startswith("Number of complete sols:"):
                num_complete_sols = int(line.split(":", 1)[1].strip())

            elif line == "Depths:":
                section = "depths"

            elif line == "Swaps:":
                section = "swaps"

            elif section in ("depths", "swaps") and line:
                parts = line.split()

                if len(parts) == 2:
                    try:
                        value, solutions = map(int, parts)

                        if section == "depths":
                            depths.append((value, solutions))
                        else:
                            swaps.append((value, solutions))

                    except ValueError:
                        pass

    return {
        "instance": instance,
        "physical_qubits": physical_qubits,
        "logical_qubits": logical_qubits,
        "optimization": optimization,
        "num_complete_sols": num_complete_sols,
        "depths": depths,
        "swaps": swaps,
    }


def make_vector(values):
    if not values:
        return []

    max_value = max(value for value, _ in values)

    vector = [0] * (max_value + 1)

    for value, solutions in values:
        vector[value] = solutions

    return vector


def save_data(values, filename):
    with open(filename, "w") as f:
        for value, solutions in values:
            f.write(f"{value} {solutions}\n")


def plot_on_axis(ax, data, values, xlabel, label):
    vector = make_vector(values)

    nonzero_values = [
        (value, solutions)
        for value, solutions in values
        if solutions > 0
    ]

    if not nonzero_values:
        ax.text(0.5, 0.5, "No non-zero values", ha="center", va="center")
        return

    x, y = zip(*nonzero_values)

    num_nonzero = len(nonzero_values)

    ax.plot(x, y, ".")

    ax.set_xlabel(xlabel)
    ax.set_ylabel("# solutions")
    ax.grid()

    ax.set_title(
        f"{data['instance']} — Optimizing_{data['optimization']}\n"
        f"Physical qubits: {data['physical_qubits']} | "
        f"Logical qubits: {data['logical_qubits']}\n"
        f"Complete solutions: {data['num_complete_sols']} | "
        f"{num_nonzero} different values found",
        fontsize=9
    )

    ax.text(
        0.02, 0.97,
        f"({label})",
        transform=ax.transAxes,
        fontsize=12,
        fontweight="bold",
        va="top"
    )


def run_benchmark(qasm_file, log_file):
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = OMP_THREADS

    command = [
        EXECUTABLE,
        "--search", "j",
        "--sabre-runs", "1",
        "--topology", "albatroz",
        "--qasmfile", str(qasm_file),
        "--depth-percent", "0.6",
        "--num-rand-sols", "1",
    ]

    print(f"Running: {qasm_file.name}")

    with open(log_file, "w") as log:
        subprocess.run(
            command,
            stdout=log,
            stderr=subprocess.STDOUT,
            env=env,
            check=True
        )


def main():

    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} QASM_FOLDER")
        sys.exit(1)

    qasm_folder = Path(sys.argv[1]).expanduser().resolve()

    if not qasm_folder.is_dir():
        print(f"Error: folder does not exist: {qasm_folder}")
        sys.exit(1)

    if not Path(EXECUTABLE).exists():
        print(f"Error: executable not found: {EXECUTABLE}")
        sys.exit(1)

    qasm_files = sorted(qasm_folder.glob("*.qasm"))

    if not qasm_files:
        print(f"No .qasm files found in {qasm_folder}")
        sys.exit(1)

    output_dir = Path("plot_data")
    output_dir.mkdir(exist_ok=True)

    results = []

    # ---------------------------------------------------------
    # Run all benchmarks
    # ---------------------------------------------------------

    for qasm_file in qasm_files:

        log_file = qasm_file.with_suffix(".LOG")

        run_benchmark(qasm_file, log_file)

        data = parse_log(log_file)
        results.append(data)

        depth_data = (
            output_dir /
            f"{data['instance']}_Optimizing_{data['optimization']}_DEPTH.dat"
        )

        swaps_data = (
            output_dir /
            f"{data['instance']}_Optimizing_{data['optimization']}_SWAPS.dat"
        )

        save_data(data["depths"], depth_data)
        save_data(data["swaps"], swaps_data)

    # ---------------------------------------------------------
    # Create one PDF containing everything
    # ---------------------------------------------------------

    pdf_file = output_dir / "all_results.pdf"

    labels = "abcdefghijklmnopqrstuvwxyz"

    with PdfPages(pdf_file) as pdf:

        panel_number = 0

        # Each instance has two panels:
        # DEPTH and SWAPS

        for start in range(0, len(results), 2):

            panels = []

            for data in results[start:start + 2]:
                panels.append((data, data["depths"], "DEPTH value"))
                panels.append((data, data["swaps"], "SWAP value"))

            fig, axes = plt.subplots(
                2,
                2,
                figsize=(11, 8.5)
            )

            axes = axes.flatten()

            for i, (data, values, xlabel) in enumerate(panels):

                if panel_number >= len(labels):
                    label = f"panel {panel_number + 1}"
                else:
                    label = labels[panel_number]

                plot_on_axis(
                    axes[i],
                    data,
                    values,
                    xlabel,
                    label
                )

                panel_number += 1

            # Hide unused axes
            for i in range(len(panels), len(axes)):
                axes[i].axis("off")

            fig.tight_layout()

            pdf.savefig(fig, bbox_inches="tight")

            # IMPORTANT:
            # no plt.show()
            plt.close(fig)

    print()
    print("Done.")
    print()
    print(f"QASM files: {len(qasm_files)}")
    print(f"PDF:        {pdf_file}")


if __name__ == "__main__":
    main()
