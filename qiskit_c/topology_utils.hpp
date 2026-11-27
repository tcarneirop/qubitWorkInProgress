#ifndef TOPOLOGY_UTILS_HPP
#define TOPOLOGY_UTILS_HPP


#include <qiskit.h>
#include <string>
#include <stdexcept>

struct PythonTarget {
    QkTarget *target;
    PyObject *owner;
};

inline PythonTarget load_python_target(const std::string &topology)
{
    const std::string name = "fake_" + topology;

    PyObject *module = PyImport_ImportModule(
        "qiskit_ibm_runtime.fake_provider");

    if (!module) {
        PyErr_Print();
        throw std::runtime_error("Cannot import fake_provider");
    }

    PyObject *cls = PyObject_GetAttrString(
        module, "FakeProviderForBackendV2");

    Py_DECREF(module);

    if (!cls) {
        PyErr_Print();
        throw std::runtime_error("Cannot find backend provider");
    }

    PyObject *provider = PyObject_CallNoArgs(cls);
    Py_DECREF(cls);

    if (!provider) {
        PyErr_Print();
        throw std::runtime_error("Cannot initialize backend provider");
    }

    PyObject *backend = PyObject_CallMethod(
        provider, "backend", "s", name.c_str());

    Py_DECREF(provider);

    if (!backend) {
        PyErr_Print();
        throw std::runtime_error(
            "Cannot load topology: " + topology);
    }

    PyObject *obj = PyObject_GetAttrString(backend, "target");
    Py_DECREF(backend);

    if (!obj) {
        PyErr_Print();
        throw std::runtime_error("Cannot retrieve target");
    }

    QkTarget *borrowed = qk_target_borrow_from_python(obj);

    QkTarget *target = borrowed;

    if (!target) {
        PyErr_Print();
        throw std::runtime_error("Cannot create QkTarget");
    }

    return {target, obj};
}

#endif