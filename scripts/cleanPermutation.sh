#!/bin/bash

printf '%s\n' "$*" |
awk '{
    gsub(/[\[\],]/, " ")
    for (i = 1; i <= NF; i++)
        printf "%s%s", $i, (i == NF ? "\n" : " ")
}'