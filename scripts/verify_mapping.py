#!/usr/bin/env python3

import csv
import ast
import sys
from collections import Counter


def check_csv(filename):
    total = 0
    warnings = 0

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)

        for line_number, row in enumerate(reader, start=2):
            total += 1

            instance = row["instance"]
            mapping_str = row["mapping"]

            try:
                mapping = ast.literal_eval(mapping_str)
            except (ValueError, SyntaxError):
                warnings += 1
                print(
                    f"WARNING: instance={instance}, line={line_number}\n"
                    f"        could not parse mapping: {mapping_str}"
                )
                continue

            counts = Counter(mapping)
            duplicated = sorted(
                value for value, count in counts.items()
                if count > 1
            )

            if duplicated:
                warnings += 1

                print(
                    f"WARNING: instance={instance}, line={line_number}\n"
                    f"        duplicated values: {duplicated}\n"
                    f"        mapping = {mapping}"
                )

    print()
    print("========================================")
    print("Mapping validation summary")
    print("========================================")
    print(f"Checked:  {total}")
    print(f"Valid:    {total - warnings}")
    print(f"Warnings: {warnings}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <csv_file>")
        sys.exit(1)

    check_csv(sys.argv[1])
