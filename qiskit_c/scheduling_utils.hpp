#ifndef SCHEDULING_UTILS_HPP
#define SCHEDULING_UTILS_HPP

#include <qiskit.h>
#include <stdexcept>

inline void init_scheduling()
{
    const int status = PyRun_SimpleString(R"PY(
from qiskit.converters import dag_to_circuit
from qiskit.transpiler.preset_passmanagers.common import generate_scheduling

def schedule_circuit(dag, target, method):
    circuit = dag_to_circuit(dag)

    pm = generate_scheduling(
        target.durations(),
        method,
        target.timing_constraints(),
        target=target
    )
    #print("Scheduling passes:", pm.to_flow_controller().tasks, flush=True)
    return pm.run(circuit)
)PY");

    if (status != 0)
        throw std::runtime_error("Could not initialize scheduling");
}

inline PyObject *run_scheduling(
    PyObject *py_dag,
    PyObject *py_target,
    const char *method = "asap")
{
    PyObject *module = PyImport_AddModule("__main__");
    PyObject *dict = PyModule_GetDict(module);

    PyObject *func = PyDict_GetItemString(
        dict, "schedule_circuit");

    if (!func || !PyCallable_Check(func))
        throw std::runtime_error("Scheduling function not available");

    PyObject *result = PyObject_CallFunction(
        func, "OOs", py_dag, py_target, method);

    if (!result) {
        PyErr_Print();
        throw std::runtime_error("Scheduling failed");
    }

    return result;
}

inline double estimate_makespan(
    PyObject *scheduled,
    PyObject *py_target)
{
    PyObject *result = PyObject_CallMethod(
        scheduled, "estimate_duration", "Os",
        py_target, "s");

    if (!result) {
        PyErr_Print();
        throw std::runtime_error("Could not estimate makespan");
    }

    const double duration = PyFloat_AsDouble(result);
    Py_DECREF(result);

    if (PyErr_Occurred()) {
        PyErr_Print();
        throw std::runtime_error("Invalid makespan");
    }

    return duration;
}

#endif