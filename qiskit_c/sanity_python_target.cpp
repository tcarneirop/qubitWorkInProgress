#define QISKIT_PYTHON_EXTENSION

#include <qiskit.h>
#include <iostream>

int main()
{
    Py_Initialize();

    if (qk_import() != 0) {
        PyErr_Print();
        Py_Finalize();
        return 1;
    }


    PyRun_SimpleString(
        "from qiskit_ibm_runtime.fake_provider import "
        "FakeProviderForBackendV2\n"
        "backend = FakeProviderForBackendV2().backend('fake_washington')\n"
        "target = backend.target\n"
    );

    PyObject *module = PyImport_AddModule("__main__");
    PyObject *dict = PyModule_GetDict(module);
    PyObject *obj = PyDict_GetItemString(dict, "target");

    if (!obj) {
        std::cerr << "Python target not found\n";
        Py_Finalize();
        return 1;
    }

    QkTarget *target = qk_target_borrow_from_python(obj);

    if (!target) {
        PyErr_Print();
        Py_Finalize();
        return 1;
    }

    std::cout << "Qubits: " << qk_target_num_qubits(target) << '\n';
    std::cout << "dt: " << qk_target_dt(target) << '\n';

    Py_Finalize();

    return 0;
}
