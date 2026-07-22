#!/bin/bash

###############################################################################
# Defaults
###############################################################################

BINARY=""
CSV="results.csv"
OUTDIR="outputs"

DEPTHS=()
POOLS=()
SABRE_RUNS=()
TIME_LIMITS=()

CONTINUE=false

###############################################################################
# Parse arguments
###############################################################################

while [[ $# -gt 0 ]]; do
    case "$1" in
        --binary)
            BINARY="$2"
            shift 2
            ;;
        --depths)
            IFS=',' read -ra DEPTHS <<< "$2"
            shift 2
            ;;
        --pool)
            IFS=',' read -ra POOLS <<< "$2"
            shift 2
            ;;
        --sabre)
            IFS=',' read -ra SABRE_RUNS <<< "$2"
            shift 2
            ;;
        --timelimits)
            IFS=',' read -ra TIME_LIMITS <<< "$2"
            shift 2
            ;;
        --csv)
            CSV="$2"
            shift 2
            ;;
        --continue)
            CONTINUE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

###############################################################################
# Validation
###############################################################################

[[ -z "$BINARY" ]] && { echo "Missing --binary"; exit 1; }
[[ ! -x "$BINARY" ]] && { echo "Binary '$BINARY' not found or not executable."; exit 1; }

[[ ${#DEPTHS[@]} -eq 0 ]] && { echo "Missing --depths"; exit 1; }
[[ ${#POOLS[@]} -eq 0 ]] && { echo "Missing --pool"; exit 1; }
[[ ${#SABRE_RUNS[@]} -eq 0 ]] && { echo "Missing --sabre"; exit 1; }
[[ ${#TIME_LIMITS[@]} -eq 0 ]] && { echo "Missing --timelimits"; exit 1; }

mkdir -p "$OUTDIR"

###############################################################################
# CSV
###############################################################################

if [[ ! -f "$CSV" ]]; then
    echo "executable,timeout,instance,number_of_sabre,pool_percent,initial_depth,execution_time,depth,gates,mapping,status" > "$CSV"
fi

###############################################################################
# Load previous executions
###############################################################################

declare -A DONE

if $CONTINUE; then
    while IFS=',' read -r executable timeout instance sabre pool depth runtime best_depth best_gates mapping status
    do
        [[ "$instance" == "instance" ]] && continue
        [[ "$instance" == "" ]] && continue

        key="$instance|$timeout|$depth|$pool|$sabre"
        DONE["$key"]=1
    done < "$CSV"
fi

###############################################################################
# Experiments
###############################################################################

for file in NEW_Bechmark/*.qasm
do
    instance=$(basename "$file" .qasm)

    for TIME_LIMIT in "${TIME_LIMITS[@]}"
    do
        for DEPTH in "${DEPTHS[@]}"
        do
            for POOL in "${POOLS[@]}"
            do
                for NUM_SABRE in "${SABRE_RUNS[@]}"
                do

                    key="$instance|$TIME_LIMIT|$DEPTH|$POOL|$NUM_SABRE"

                    if $CONTINUE && [[ -n "${DONE[$key]}" ]]; then
                        echo "Skipping $key"
                        continue
                    fi

                    outfile="${OUTDIR}/${instance}_d${DEPTH}_p${POOL}_s${NUM_SABRE}_t${TIME_LIMIT}.out"

                    echo "=================================================="
                    echo "Instance : $instance"
                    echo "Depth    : $DEPTH"
                    echo "Pool     : $POOL"
                    echo "Sabre    : $NUM_SABRE"
                    echo "Timeout  : $TIME_LIMIT"
                    echo "Started  : $(date)"

                    start=$(date +%s)

                    stdbuf -o0 -e0 \
                        timeout "$TIME_LIMIT" \
                        "$BINARY" \
                        "$file" \
                        16 \
                        "$DEPTH" \
                        "$POOL" \
                        "$NUM_SABRE" \
                        > "$outfile" 2>&1

                    exitcode=$?

                    end=$(date +%s)
                    elapsed=$((end-start))

                    case $exitcode in
                        0)
                            status="SUCCESS"
                            ;;
                        124)
                            status="TIMEOUT"
                            ;;
                        *)
                            status="ERROR($exitcode)"
                            ;;
                    esac

                    best_depth=$(grep "Depth:" "$outfile" | tail -n1 | sed 's/.*Depth:[[:space:]]*//')
                    best_gates=$(grep "Num gates:" "$outfile" | tail -n1 | sed 's/.*Num gates:[[:space:]]*//')
                    best_mapping=$(grep "Mapping:" "$outfile" | tail -n1 | sed 's/.*Mapping:[[:space:]]*//')

                    echo "\"$BINARY\",\"$TIME_LIMIT\",\"$instance\",$NUM_SABRE,$POOL,$DEPTH,$elapsed,\"$best_depth\",\"$best_gates\",\"$best_mapping\",$status" >> "$CSV"

                done
            done
        done
    done
done

echo
echo "Finished."