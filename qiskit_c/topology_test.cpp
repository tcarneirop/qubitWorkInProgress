#define QISKIT_PYTHON_EXTENSION

#include "topology_utils.hpp"
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <topology>\n";
        return 1;
    }

    Py_Initialize();

    if (qk_import() != 0) {
        PyErr_Print();
        Py_Finalize();
        return 1;
    }

    int status = 0;

    try {
        PythonTarget loaded = load_python_target(argv[1]);
        QkTarget *target = loaded.target;

        std::cout << "Topology: " << argv[1] << '\n';
        std::cout << "Qubits: " << qk_target_num_qubits(target) << '\n';
        std::cout << "dt: " << qk_target_dt(target) << '\n';
        Py_DECREF(loaded.owner);
        //qk_target_free(target);
    }
    catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        status = 1;
    }

    
    Py_Finalize();


    return status;
}