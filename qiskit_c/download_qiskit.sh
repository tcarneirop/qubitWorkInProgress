#!/bin/bash
mkdir -p ~/qubitWorkInProgress
cd ~/qubitWorkInProgress

git clone --branch 2.6.2 --depth 1 \
    https://github.com/Qiskit/qiskit.git

cd qiskit
git describe --tags --exact-match
