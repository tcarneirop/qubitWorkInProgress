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

# Test the Python-backed API using the installed package's own vtables.
# Do not link the standalone libqiskit.so or use its patched headers here.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QISKIT_VENV="${QISKIT_VENV:-${VIRTUAL_ENV:-$SCRIPT_DIR/.venv}}"
if [[ ! -x "$QISKIT_VENV/bin/python" ]]; then
    echo "ERROR: Python environment not found: $QISKIT_VENV" >&2
    exit 1
fi
source "$QISKIT_VENV/bin/activate"
PYTHON_CONFIG="${PYTHON_CONFIG:-python3.12-config}"
python -I -c 'import sys, qiskit, qiskit_ibm_runtime; assert sys.version_info[:2] == (3, 12); assert qiskit.__version__ == "2.6.2"; assert qiskit_ibm_runtime.__version__ == "0.52.1"'
PYTHON_SITE="$(python -I -c 'import pathlib, qiskit; print(pathlib.Path(qiskit.__file__).resolve().parent.parent)')"
PYTHON_QISKIT_INCLUDE="$PYTHON_SITE/qiskit/capi/include"
if [[ ! -f "$PYTHON_QISKIT_INCLUDE/qiskit/funcs_py_generated.h" ]]; then
    echo "ERROR: Python-backed Qiskit C API headers not found" >&2
    exit 1
fi
read -r -a PY_INCLUDES <<< "$($PYTHON_CONFIG --includes)"
read -r -a PY_LDFLAGS <<< "$($PYTHON_CONFIG --embed --ldflags)"
cat > "$TEMP_DIR/check_python.cpp" <<'CPP'
#define QISKIT_PYTHON_EXTENSION
#include <qiskit.h>
#include <cmath>
#include <iostream>

int main() {
    Py_Initialize();
    if (qk_import() != 0 || PyRun_SimpleString(
        "from qiskit_ibm_runtime.fake_provider import FakeProviderForBackendV2\n"
        "backend = FakeProviderForBackendV2().backend('fake_washington')\n"
        "target = backend.target\n") != 0) {
        if (PyErr_Occurred()) PyErr_Print();
        Py_Finalize();
        return 1;
    }
    PyObject *module = PyImport_AddModule("__main__");
    PyObject *obj = module ? PyDict_GetItemString(PyModule_GetDict(module), "target") : nullptr;
    QkTarget *target = obj ? qk_target_borrow_from_python(obj) : nullptr;
    if (!target) {
        if (PyErr_Occurred()) PyErr_Print();
        std::cerr << "ERROR: Could not borrow Python Target\n";
        Py_Finalize();
        return 1;
    }
    const auto options = qk_transpiler_default_options();
    if (options.num_trials != 5) {
        std::cerr << "ERROR: Python-backed num_trials default is not 5\n";
        Py_Finalize();
        return 1;
    }
    std::cout << "PASS: Python-backed num_trials (default=5).\n";
    const auto n = qk_target_num_qubits(target);
    const auto dt = qk_target_dt(target);
    std::cout << "Qubits: " << n << "\ndt: " << dt << '\n';
    const bool ok = n == 127 && std::abs(dt - 2.2222222222222221e-10) < 1e-18;
    // The target belongs to Python: do not call qk_target_free().
    Py_Finalize();
    if (!ok) return 1;
    std::cout << "PASS: Python-backed Qiskit C API (Washington).\n";
    return 0;
}
CPP

g++ -O2 -std=c++17 -Wall -Wextra \
    -I"$PYTHON_QISKIT_INCLUDE" "${PY_INCLUDES[@]}" \
    "$TEMP_DIR/check_python.cpp" -o "$TEMP_DIR/check_python.exe" \
    "${PY_LDFLAGS[@]}"

# Make venv packages visible to embedded Python and avoid source-checkout shadowing.
(cd "$TEMP_DIR" && PYTHONPATH="$PYTHON_SITE" ./check_python.exe)

# Exercise the actual shared scheduling helper with the validated Washington circuit.
cat > "$TEMP_DIR/check_scheduling.cpp" <<'CPP'
#define QISKIT_PYTHON_EXTENSION
#include <qiskit.h>
#include "scheduling_utils.hpp"
#include <cmath>
#include <iostream>
#include <memory>
int main() {
    Py_Initialize();
    int rc = 1;
    try {
        if (qk_import() != 0) throw std::runtime_error("qk_import failed");
        if (PyRun_SimpleString(
            "from qiskit_ibm_runtime.fake_provider import FakeProviderForBackendV2\n"
            "from qiskit import QuantumCircuit\n"
            "from qiskit.converters import circuit_to_dag\n"
            "target = FakeProviderForBackendV2().backend('fake_washington').target\n"
            "qc = QuantumCircuit(127)\n"
            "qc.x(0); qc.cx(0,1); qc.x(1)\n"
            "dag = circuit_to_dag(qc)\n") != 0)
            throw std::runtime_error("Could not create scheduling test circuit");
        init_scheduling();
        PyObject* dict = PyModule_GetDict(PyImport_AddModule("__main__"));
        PyObject* target = PyDict_GetItemString(dict, "target");
        PyObject* dag = PyDict_GetItemString(dict, "dag");
        for (const char* method : {"asap", "alap"}) {
            auto deleter = [](PyObject* obj) { Py_XDECREF(obj); };
            std::unique_ptr<PyObject, decltype(deleter)> scheduled(
                run_scheduling(dag, target, method), deleter);
            double duration = estimate_makespan(scheduled.get(), target);
            if (!std::isfinite(duration) || std::abs(duration - 924.4444444444443e-9) > 1e-12)
                throw std::runtime_error("Unexpected Washington makespan");
            std::cout << "PASS: " << method << " makespan (s): " << duration << '\n';
        }
        rc = 0;
    } catch (const std::exception& e) {
        if (PyErr_Occurred()) PyErr_Print();
        std::cerr << e.what() << '\n';
    }
    Py_Finalize();
    return rc;
}
CPP
g++ -O2 -std=c++17 -Wall -Wextra \
    -I"$PYTHON_QISKIT_INCLUDE" -I"$SCRIPT_DIR" "${PY_INCLUDES[@]}" \
    "$TEMP_DIR/check_scheduling.cpp" -o "$TEMP_DIR/check_scheduling.exe" \
    "${PY_LDFLAGS[@]}"
(cd "$TEMP_DIR" && PYTHONPATH="$PYTHON_SITE" ./check_scheduling.exe)
