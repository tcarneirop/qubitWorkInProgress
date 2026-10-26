#!/bin/bash

LOG="$1"
OUTPUT="${LOG%.log}_swap_counts.dat"

grep -E '^[[:space:]]*Num swaps:[[:space:]]*[0-9]+' "$LOG" |
    sed -E 's/^[[:space:]]*Num swaps:[[:space:]]*([0-9]+).*$/\1/' |
    sort -n |
    uniq -c |
    awk '{print $2, $1}' > "$OUTPUT"

echo "Dados salvos em: $OUTPUT"
