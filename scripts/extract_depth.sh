#!/bin/bash

LOG="$1"
OUTPUT="${LOG%.log}_depth_counts.dat"

grep -E '^[[:space:]]*Depth:[[:space:]]*[0-9]+' "$LOG" |
    sed -E 's/^[[:space:]]*Depth:[[:space:]]*([0-9]+).*$/\1/' |
    sort -n |
    uniq -c |
    awk '{print $2, $1}' > "$OUTPUT"

echo "Dados salvos em: $OUTPUT"
