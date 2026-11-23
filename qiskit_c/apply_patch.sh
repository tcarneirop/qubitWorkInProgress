#/bin/bash

cd ~/qubitWorkInProgress/qiskit

git apply --check ../qiskit_c/qiskit-2.6.2-sabre-routing.patch

git apply ../qiskit_c/qiskit-2.6.2-sabre-routing.patch

git diff --stat


cd ~/qubitWorkInProgress/qiskit
make c