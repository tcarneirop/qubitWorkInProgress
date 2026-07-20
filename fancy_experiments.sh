#!/bin/bash

TIME_LIMIT="7200s"

for file in NEW_Bechmark/*.qasm
do
    name=$(basename "$file" .qasm)

    echo "=================================================="
    echo "Running $name ($(date))"

    stdbuf -o0 -e0 \
        timeout "$TIME_LIMIT" \
        ./qubit.exe "$file" 16 7 1 \
        > "${name}.out" 2>&1

    status=$?

    case $status in
        0)
            echo "$name finished successfully."
            ;;
        124)
            echo "$name timed out after $TIME_LIMIT."
            ;;
        *)
            echo "$name failed with status $status."
            ;;
    esac
done

echo "All instances processed."
