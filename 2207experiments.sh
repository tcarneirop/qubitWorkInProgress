#!/bin/bash

###############################################################################
# Defaults
###############################################################################

BINARY=""
CSV="2207results.csv"
OUTDIR="outputs"

DEPTHS=()
POOLS=()
SABRE_RUNS=()
TIME_LIMITS=()
NUMTHREADS=($(nproc))
CONTINUE=false
LIKWID=false

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
        --numthreads)
            IFS=',' read -ra NUMTHREADS <<< "$2"
            shift 2
            ;;
        --likwid)
            LIKWID=true
            shift
            ;;
        --continue)
            CONTINUE=true
            shift
            ;;

        --help|-h)
            cat << EOF
Usage:
  $0 [OPTIONS]

Options:
  --binary <path>          Executable to run
  --depths <list>          Percent of the partial permutation that is going to be fixed.
                           Example: --depths 0.2,0.3,0.4,0.5 
                           Obs: 0.5 can be VERY memory demanding for some instances.
  --pool <list>            Pool percentages, comma-separated
                           Example: 1,0.1,0.01,0.001, and one experiment is executed per depth
  --sabre <list>           Number of SABRE runs, comma-separated
                           Example: 1,5,10
  --timelimits <list>      Time limits, comma-separated
                           Example: 10s,60s,1200s
  --numthreads <n>         Number of OpenMP threads.
                           Example:  --numthreads 1,2,4,8,16 overrides the default and runs one experiment per parameter.
  --likwid                 Run using LIKWID to pin threads
  --csv <file>             CSV output file
  --continue               Skip experiments already present in CSV
  -h, --help               Show this help message

Example:
  $0 --binary ./qubit.exe \\
     --depths 0.25,0.30 \\
     --pool 1 \\
     --sabre 1,10 \\
     --timelimits 5s,10s,60S \\
     --csv 10sresults.csv \\
     --numthreads 256 \\
     --continue
EOF
            exit 0
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
   echo "executable,timeout,instance,number_of_sabre,pool_percent,initial_depth,num_threads,execution_time,depth,gates,mapping,solutions,status" > "$CSV"
fi

###############################################################################
# Likwid check
###############################################################################

if $LIKWID; then
    if ! command -v likwid-pin &> /dev/null; then
        echo "Error: --likwid was specified, but 'likwid-pin' was not found in PATH."
        exit 1
    fi
fi

###############################################################################
# Load previous executions
###############################################################################

declare -A DONE

if $CONTINUE; then

    # pula o cabeçalho
    tail -n +2 "$CSV" | while IFS= read -r line
    do
        # pega somente as seis primeiras colunas
        first6=$(echo "$line" | cut -d',' -f1-6)

        IFS=',' read -r executable timeout instance sabre pool depth <<< "$first6"

        # remove aspas
        executable=${executable//\"/}
        timeout=${timeout//\"/}
        instance=${instance//\"/}

        key="$instance|$timeout|$depth|$pool|$sabre"

        DONE["$key"]=1
    done

    # como o while acima roda em um subshell por causa do pipe,
    # fazemos a leitura novamente usando redirecionamento.

    unset DONE
    declare -A DONE

    {
        read    # cabeçalho

        while IFS= read -r line
        do
            first6=$(echo "$line" | cut -d',' -f1-6)

            IFS=',' read -r executable timeout instance sabre pool depth <<< "$first6"

            executable=${executable//\"/}
            timeout=${timeout//\"/}
            instance=${instance//\"/}

            key="$instance|$timeout|$depth|$pool|$sabre"

            DONE["$key"]=1
        done
    } < "$CSV"

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

                    #outfile="${OUTDIR}/${instance}_d${DEPTH}_p${POOL}_s${NUM_SABRE}_t${TIME_LIMIT}.out"

                    DATE=$(date +%Y%m%d)

                    RESULT_DIR="results/${DATE}_d${DEPTH}_p${POOL}_s${NUM_SABRE}_t${TIME_LIMIT}/${instance}"
                    mkdir -p "$RESULT_DIR"

                    outfile="${RESULT_DIR}/${instance}.out"

                    echo "=================================================="
                    echo "Instance : $instance"
                    echo "Depth    : $DEPTH"
                    echo "Pool     : $POOL"
                    echo "Sabre    : $NUM_SABRE"
                    echo "Timeout  : $TIME_LIMIT"
                    echo "Threads  : $NUMTHREADS"
                    ECHO "Likwid   : $LIKWID"
                    echo "Started  : $(date)"

                    start=$(date +%s)
                    export OMP_NUM_THREADS=${NUMTHREADS}

                if $LIKWID; then
                    LIKWID_CMD=(likwid-pin -c "0-$((NUMTHREADS - 1))")
                else
                    LIKWID_CMD=()
                fi

                stdbuf -o0 -e0 \
                    timeout "$TIME_LIMIT" \
                    "${LIKWID_CMD[@]}" "$BINARY" \
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
                    #solutions=$(grep "Solution:" "$outfile" | tail -n1 | sed 's/.*Solution:[[:space:]]*//')
		            solutions=$(grep "Solution:" "$outfile" | tail -n1 | sed 's/.*Solution:[[:space:]]*//' | sed 's/,.*//')

                    #echo "\"$BINARY\",\"$TIME_LIMIT\",\"$instance\",$NUM_SABRE,$POOL,$DEPTH,$elapsed,\"$best_depth\",\"$best_gates\",\"$best_mapping\",$status" >> "$CSV"
                    echo "\"$BINARY\",\"$TIME_LIMIT\",\"$instance\",$NUM_SABRE,$POOL,$DEPTH,$NUMTHREADS,$elapsed,\"$best_depth\",\"$best_gates\",\"$best_mapping\",\"$solutions\",$status" >> "$CSV"
                done
            done
        done
    done
done

echo
echo "Finished."
