#!/usr/bin/env python3

import csv
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path


# ============================================================
# Configuration
# ============================================================

QUBIT_EXE = "./qubit.exe"
CLEAN_PERMUTATION = "./scripts/cleanPermutation.sh"
QASM_DIR = "./NEW_Bechmark"
OUTPUT_ROOT = "./check_outputs"


# ============================================================
# Helpers
# ============================================================

def extract_value(output, name):
    """
    Extract an integer from lines such as:

        Depth: 46074
        Gates: 63744
    """

    match = re.search(
        rf"^\s*{name}:\s*(\d+)",
        output,
        re.MULTILINE,
    )

    if match:
        return int(match.group(1))

    return None


def clean_permutation(mapping):
    """
    Convert:

        [0, 13, 7, 11]

    into:

        0 13 7 11
    """

    result = subprocess.run(
        [CLEAN_PERMUTATION, mapping],
        capture_output=True,
        text=True,
        check=True,
    )

    return result.stdout.strip()


def check_permutation(
    instance,
    number_of_sabre,
    mapping,
    output_file,
):
    """
    Run qubit.exe using the instance, SABRE runs and
    permutation from the CSV.

    The complete stdout + stderr is saved to output_file.
    """

    permutation = clean_permutation(mapping)

    qasmfile = Path(QASM_DIR) / f"{instance}.qasm"

    command = [
        QUBIT_EXE,
        "--topology", "albatroz",
        "--qasmfile", str(qasmfile),
        "--sabre-runs", str(number_of_sabre),
        "--permutation", *permutation.split(),
    ]

    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
    )

    output = result.stdout + "\n" + result.stderr

    with open(
        output_file,
        "w",
        encoding="utf-8",
    ) as f:
        f.write(output)

    depth = extract_value(output, "Depth")
    gates = extract_value(output, "Gates")

    return result.returncode, depth, gates


# ============================================================
# Main
# ============================================================

if len(sys.argv) != 3:
    print(
        f"Usage: {sys.argv[0]} input.csv output.csv",
        file=sys.stderr,
    )
    sys.exit(1)


input_csv = sys.argv[1]
output_csv_name = sys.argv[2]


# ============================================================
# Create timestamped output directory
# ============================================================

timestamp = datetime.now().strftime(
    "%Y-%m-%d_%H-%M-%S"
)

output_dir = Path(OUTPUT_ROOT) / timestamp

output_dir.mkdir(
    parents=True,
    exist_ok=True,
)

output_csv = output_dir / output_csv_name


# ============================================================
# Counters
# ============================================================

total_rows = 0
rows_checked = 0
rows_skipped = 0

depth_identical = 0
depth_different = 0

gates_identical = 0
gates_different = 0

warnings = []


# ============================================================
# Read CSV
# ============================================================

with open(
    input_csv,
    newline="",
    encoding="utf-8",
) as infile:

    reader = csv.DictReader(infile)

    original_fields = reader.fieldnames

    if original_fields is None:
        print(
            "ERROR: CSV has no header.",
            file=sys.stderr,
        )
        sys.exit(1)

    required_fields = {
        "instance",
        "depth",
        "gates",
        "mapping",
        "number_of_sabre",
    }

    missing = required_fields - set(original_fields)

    if missing:
        print(
            "ERROR: missing CSV columns: "
            + ", ".join(sorted(missing)),
            file=sys.stderr,
        )
        sys.exit(1)

    output_fields = (
        ["check_id"]
        + original_fields
        + [
            "checked_depth",
            "depth_gap_percent",
            "checked_gates",
            "gates_gap_percent",
        ]
    )

    rows = []


    # ========================================================
    # Process every row
    # ========================================================

    for line_number, row in enumerate(
        reader,
        start=2,
    ):

        total_rows += 1

        instance = (
            row.get("instance") or ""
        ).strip()

        mapping = (
            row.get("mapping") or ""
        ).strip()

        number_of_sabre = (
            row.get("number_of_sabre") or ""
        ).strip()

        check_id = (
            f"{instance}_{line_number:06d}"
        )

        row["check_id"] = check_id


        # ----------------------------------------------------
        # Empty mapping
        # ----------------------------------------------------

        if not mapping:

            rows_skipped += 1

            warnings.append(
                f"line {line_number}: empty mapping "
                f"(instance={instance}, "
                f"check_id={check_id})"
            )

            row["checked_depth"] = ""
            row["depth_gap_percent"] = ""
            row["checked_gates"] = ""
            row["gates_gap_percent"] = ""

            rows.append(row)
            continue


        # ----------------------------------------------------
        # Empty SABRE runs
        # ----------------------------------------------------

        if not number_of_sabre:

            rows_skipped += 1

            warnings.append(
                f"line {line_number}: empty number_of_sabre "
                f"(instance={instance}, "
                f"check_id={check_id})"
            )

            row["checked_depth"] = ""
            row["depth_gap_percent"] = ""
            row["checked_gates"] = ""
            row["gates_gap_percent"] = ""

            rows.append(row)
            continue


        # ----------------------------------------------------
        # QASM file
        # ----------------------------------------------------

        qasmfile = (
            Path(QASM_DIR) /
            f"{instance}.qasm"
        )

        if not qasmfile.exists():

            rows_skipped += 1

            warnings.append(
                f"line {line_number}: QASM file not found "
                f"(instance={instance}, "
                f"path={qasmfile}, "
                f"check_id={check_id})"
            )

            row["checked_depth"] = ""
            row["depth_gap_percent"] = ""
            row["checked_gates"] = ""
            row["gates_gap_percent"] = ""

            rows.append(row)
            continue


        # ----------------------------------------------------
        # Raw output file
        # ----------------------------------------------------

        output_file = (
            output_dir /
            f"{check_id}.txt"
        )


        # ----------------------------------------------------
        # Run qubit.exe
        # ----------------------------------------------------

        try:

            (
                returncode,
                checked_depth,
                checked_gates,
            ) = check_permutation(
                instance,
                number_of_sabre,
                mapping,
                output_file,
            )

        except Exception as e:

            rows_skipped += 1

            warnings.append(
                f"line {line_number}: failed to run "
                f"{instance}: {e} "
                f"(check_id={check_id})"
            )

            row["checked_depth"] = ""
            row["depth_gap_percent"] = ""
            row["checked_gates"] = ""
            row["gates_gap_percent"] = ""

            rows.append(row)
            continue


        # ----------------------------------------------------
        # Return code
        # ----------------------------------------------------

        if returncode != 0:

            warnings.append(
                f"line {line_number}: qubit.exe returned "
                f"exit code {returncode} "
                f"(instance={instance}, "
                f"check_id={check_id}, "
                f"output={output_file})"
            )


        # ----------------------------------------------------
        # Missing Depth
        # ----------------------------------------------------

        if checked_depth is None:

            warnings.append(
                f"line {line_number}: Depth not found "
                f"(instance={instance}, "
                f"check_id={check_id}, "
                f"output={output_file})"
            )


        # ----------------------------------------------------
        # Missing Gates
        # ----------------------------------------------------

        if checked_gates is None:

            warnings.append(
                f"line {line_number}: Gates not found "
                f"(instance={instance}, "
                f"check_id={check_id}, "
                f"output={output_file})"
            )


        # ----------------------------------------------------
        # Original Depth
        # ----------------------------------------------------

        try:

            csv_depth = float(
                row["depth"]
            )

        except (
            ValueError,
            TypeError,
        ):

            csv_depth = None

            warnings.append(
                f"line {line_number}: invalid depth "
                f"(instance={instance}, "
                f"check_id={check_id})"
            )


        # ----------------------------------------------------
        # Original Gates
        # ----------------------------------------------------

        try:

            csv_gates = float(
                row["gates"]
            )

        except (
            ValueError,
            TypeError,
        ):

            csv_gates = None

            warnings.append(
                f"line {line_number}: invalid gates "
                f"(instance={instance}, "
                f"check_id={check_id})"
            )


        # ----------------------------------------------------
        # Count checked rows
        # ----------------------------------------------------

        rows_checked += 1


        # ----------------------------------------------------
        # Checked values
        # ----------------------------------------------------

        row["checked_depth"] = (
            checked_depth
            if checked_depth is not None
            else ""
        )

        row["checked_gates"] = (
            checked_gates
            if checked_gates is not None
            else ""
        )


        # ----------------------------------------------------
        # Depth comparison
        # ----------------------------------------------------

        if (
            checked_depth is not None
            and csv_depth is not None
        ):

            if checked_depth == csv_depth:
                depth_identical += 1
            else:
                depth_different += 1

            if csv_depth != 0:

                depth_gap = (
                    (checked_depth - csv_depth)
                    / csv_depth
                    * 100
                )

                row["depth_gap_percent"] = (
                    f"{depth_gap:.3f}"
                )

            else:

                row["depth_gap_percent"] = ""

        else:

            row["depth_gap_percent"] = ""


        # ----------------------------------------------------
        # Gates comparison
        # ----------------------------------------------------

        if (
            checked_gates is not None
            and csv_gates is not None
        ):

            if checked_gates == csv_gates:
                gates_identical += 1
            else:
                gates_different += 1

            if csv_gates != 0:

                gates_gap = (
                    (checked_gates - csv_gates)
                    / csv_gates
                    * 100
                )

                row["gates_gap_percent"] = (
                    f"{gates_gap:.3f}"
                )

            else:

                row["gates_gap_percent"] = ""

        else:

            row["gates_gap_percent"] = ""


        rows.append(row)


# ============================================================
# Write output CSV
# ============================================================

with open(
    output_csv,
    "w",
    newline="",
    encoding="utf-8",
) as outfile:

    writer = csv.DictWriter(
        outfile,
        fieldnames=output_fields,
    )

    writer.writeheader()
    writer.writerows(rows)


# ============================================================
# Calculate summary percentages
# ============================================================

depth_comparable = (
    depth_identical + depth_different
)

gates_comparable = (
    gates_identical + gates_different
)

if depth_comparable > 0:
    depth_identical_pct = (
        depth_identical
        / depth_comparable
        * 100
    )

    depth_different_pct = (
        depth_different
        / depth_comparable
        * 100
    )
else:
    depth_identical_pct = 0.0
    depth_different_pct = 0.0


if gates_comparable > 0:
    gates_identical_pct = (
        gates_identical
        / gates_comparable
        * 100
    )

    gates_different_pct = (
        gates_different
        / gates_comparable
        * 100
    )
else:
    gates_identical_pct = 0.0
    gates_different_pct = 0.0


# ============================================================
# Final Summary
# ============================================================

print()
print("#summary")

print(f"Total rows:   {total_rows}")
print(f"Rows checked: {rows_checked}")
print(f"Rows skipped: {rows_skipped}")

print()

print("Depth:")
print(
    f"  identical: {depth_identical} "
    f"({depth_identical_pct:.2f}%)"
)
print(
    f"  different: {depth_different} "
    f"({depth_different_pct:.2f}%)"
)

print()

print("Gates:")
print(
    f"  identical: {gates_identical} "
    f"({gates_identical_pct:.2f}%)"
)
print(
    f"  different: {gates_different} "
    f"({gates_different_pct:.2f}%)"
)


# ============================================================
# Warnings
# ============================================================

print()
print("#warningz")

if warnings:

    for warning in warnings:
        print(f"WARNING: {warning}")

else:

    print("No warnings.")


# ============================================================
# Output information
# ============================================================

print()
print(f"Output directory: {output_dir}")
print(f"Output CSV:       {output_csv}")