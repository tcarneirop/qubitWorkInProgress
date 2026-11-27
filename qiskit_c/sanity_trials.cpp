#define QISKIT_PYTHON_EXTENSION

#include <Python.h>
#include <qiskit.h>
#include <iostream>

int main() {
    Py_Initialize();

    if (qk_import() != 0) {
        PyErr_Print();
        Py_Finalize();
        return 1;
    }

    QkTranspileOptions options = qk_transpiler_default_options();

    std::cout << "Default num_trials: "
              << options.num_trials << '\n';

    Py_Finalize();
    return 0;
}
