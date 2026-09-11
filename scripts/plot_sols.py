import matplotlib.pyplot as plt
import re
import sys
from pathlib import Path


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

            # ---------------------------------
            # Instance
            # ---------------------------------

            if line.startswith("circuit_flat.filename:"):

                qasm = line.split(":", 1)[1].strip()

                instance = Path(qasm).stem


            # ---------------------------------
            # Qubits
            # ---------------------------------

            elif line.startswith("Physic QUBITS:"):

                match = re.search(
                    r"Physic QUBITS:\s*(\d+)\s+Logic QUBITS:\s*(\d+)",
                    line
                )

                if match:

                    physical_qubits = int(match.group(1))
                    logical_qubits = int(match.group(2))


            # ---------------------------------
            # Optimization target
            # ---------------------------------

            elif "Optimizing for DEPTH" in line:

                optimization = "DEPTH"


            elif "Optimizing for GATES" in line:

                optimization = "GATES"


            # ---------------------------------
            # Number of complete solutions
            # ---------------------------------

            elif line.startswith("Number of complete sols:"):

                num_complete_sols = int(
                    line.split(":", 1)[1].strip()
                )


            # ---------------------------------
            # Sections
            # ---------------------------------

            elif line == "Depths:":

                section = "depths"


            elif line == "Swaps:":

                section = "swaps"


            # ---------------------------------
            # Data
            # ---------------------------------

            elif section in ("depths", "swaps") and line:

                parts = line.split()

                if len(parts) == 2:

                    try:

                        value, solutions = map(int, parts)

                        if section == "depths":

                            depths.append((value, solutions))

                        elif section == "swaps":

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

    # Find the largest value present in the log
    max_value = max(
        value for value, _ in values
    )

    # Create vector from 0 to max_value
    vector = [0] * (max_value + 1)

    # Fill vector
    for value, solutions in values:

        vector[value] = solutions

    return vector


def save_data(values, filename):

    with open(filename, "w") as f:

        for value, solutions in values:

            f.write(
                f"{value} {solutions}\n"
            )

def make_plot(data, values, xlabel, output_file):
    vector = make_vector(values)

    if not vector:
        print(f"No data found for {xlabel}")
        return

    nonzero_indices = [
        i for i, solutions in enumerate(vector)
        if solutions > 0
    ]

    if not nonzero_indices:
        print(f"No non-zero values found for {xlabel}")
        return

    first_nonzero = nonzero_indices[0]
    last_nonzero = nonzero_indices[-1]

    x = range(first_nonzero, last_nonzero + 1)
    y = vector[first_nonzero:last_nonzero + 1]

    num_nonzero = len(nonzero_indices)

    plt.figure()

    plt.title(
        f"{data['instance']} — Optimizing_{data['optimization']}\n"
        f"Physical qubits: {data['physical_qubits']} | "
        f"Logical qubits: {data['logical_qubits']}\n"
        f"Complete solutions: {data['num_complete_sols']} | "
        f"{num_nonzero} different values found"
    )

    plt.plot(x, y, ".")
    plt.xlabel(xlabel)
    plt.ylabel("# solutions")
    plt.grid()

    plt.savefig(output_file, bbox_inches="tight")
    plt.close()

def main():

    # ---------------------------------
    # Command line argument
    # ---------------------------------

    if len(sys.argv) != 2:

        print(
            f"Usage: python3 {sys.argv[0]} file.log"
        )

        sys.exit(1)


    input_file = Path(
        sys.argv[1]
    )


    # ---------------------------------
    # Output directory
    # ---------------------------------

    output_dir = Path(
        "plot_data"
    )

    output_dir.mkdir(
        exist_ok=True
    )


    # ---------------------------------
    # Parse log
    # ---------------------------------

    data = parse_log(
        input_file
    )


    # ---------------------------------
    # Output filenames
    # ---------------------------------

    depth_pdf = (
        output_dir /
        f"{data['instance']}_"
        f"Optimizing_{data['optimization']}_DEPTH.pdf"
    )

    swaps_pdf = (
        output_dir /
        f"{data['instance']}_"
        f"Optimizing_{data['optimization']}_SWAPS.pdf"
    )

    depth_data = (
        output_dir /
        f"{data['instance']}_"
        f"Optimizing_{data['optimization']}_DEPTH.dat"
    )

    swaps_data = (
        output_dir /
        f"{data['instance']}_"
        f"Optimizing_{data['optimization']}_SWAPS.dat"
    )


    # ---------------------------------
    # Save extracted data
    # ---------------------------------

    save_data(
        data["depths"],
        depth_data
    )

    save_data(
        data["swaps"],
        swaps_data
    )


    # ---------------------------------
    # DEPTH plot
    # ---------------------------------

    make_plot(
        data,
        data["depths"],
        "DEPTH value",
        depth_pdf
    )


    # ---------------------------------
    # SWAPS plot
    # ---------------------------------

    make_plot(
        data,
        data["swaps"],
        "SWAP value",
        swaps_pdf
    )


    # ---------------------------------
    # Summary
    # ---------------------------------

    print()
    print("Done.")
    print()

    print(
        f"Instance:           {data['instance']}"
    )

    print(
        f"Physical qubits:    {data['physical_qubits']}"
    )

    print(
        f"Logical qubits:     {data['logical_qubits']}"
    )

    print(
        f"Optimizing for:     {data['optimization']}"
    )

    print(
        f"Complete solutions: {data['num_complete_sols']}"
    )

    print()

    print(
        f"Depth PDF:          {depth_pdf}"
    )

    print(
        f"Depth data:         {depth_data}"
    )

    print(
        f"Swaps PDF:          {swaps_pdf}"
    )

    print(
        f"Swaps data:         {swaps_data}"
    )


if __name__ == "__main__":

    main()