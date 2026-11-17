#!/usr/bin/env bash
set -euo pipefail

# Verify the Qiskit C API build and the custom SABRE Routing option.
QISKIT_HOME="${QISKIT_HOME:-$HOME/qubitWorkInProgress/qiskit}"
INCLUDE_DIR="$QISKIT_HOME/dist/c/include"
LIB_DIR="$QISKIT_HOME/dist/c/lib"

if [[ ! -f "$INCLUDE_DIR/qiskit.h" ]]; then
    echo "ERROR: Qiskit C API header not found: $INCLUDE_DIR/qiskit.h" >&2
    exit 1
fi
if [[ ! -f "$LIB_DIR/libqiskit.so" ]]; then
    echo "ERROR: Qiskit C API library not found: $LIB_DIR/libqiskit.so" >&2
    exit 1
fi

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT

# Compile a small program to verify headers, linking, runtime loading,
# and the custom num_trials option exposed by the patched Rust library.
cat > "$TEMP_DIR/check.cpp" <<'CPP'
#include <qiskit.h>
#include <iostream>

int main() {
    QkTranspileOptions options = qk_transpiler_default_options();
    if (options.num_trials != 5) {
        std::cerr << "ERROR: Expected default num_trials=5, got "
                  << options.num_trials << "\n";
        return 1;
    }
    options.num_trials = 20;
    if (options.num_trials != 20) return 1;
    std::cout << "PASS: Qiskit C API compiles, links and runs.\n";
    std::cout << "PASS: Custom SABRE num_trials field is available (default=5).\n";
    return 0;
}
CPP

g++ -O2 -std=c++17 -Wall -Wextra \
    -I"$INCLUDE_DIR" "$TEMP_DIR/check.cpp" \
    -L"$LIB_DIR" -Wl,-rpath,"$LIB_DIR" -lqiskit \
    -o "$TEMP_DIR/check.exe"

"$TEMP_DIR/check.exe"
