#!/bin/bash
set -euo pipefail

cd ~/qubitWorkInProgress/qiskit

if git apply --reverse --check ../qiskit_c/qiskit-2.6.2-sabre-routing.patch 2>/dev/null; then
    echo "Patch already applied."
else
    git apply --check ../qiskit_c/qiskit-2.6.2-sabre-routing.patch
    git apply ../qiskit_c/qiskit-2.6.2-sabre-routing.patch
fi

git diff --stat


cd ~/qubitWorkInProgress/qiskit
make c
# For the patched Python wheel and complete setup, run setup_qiskit.sh.
