#pragma once

#include "function_traits.hpp"
#include "reflect.hpp"

#include <algorithm>
#include <cassert>
#include <meta>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <Python.h>

namespace rebind {

extern "C" inline PyObject* trampoline(PyObject* self, PyObject* args, PyObject* kwargs) {
    const auto* cb = static_cast<const CallableBase*>(PyCapsule_GetPointer(self, "callable"));
    if (!cb) {
        return nullptr;
    }

    return cb->invoke(cb, args, kwargs);
}

template <typename Fn>
inline PyObject* addFunction(PyObject* module, const CallableInfo<Fn>* cb) {
    auto* def = new PyMethodDef{
        cb->getName().data(),
        reinterpret_cast<PyCFunction>(trampoline),
        METH_VARARGS | METH_KEYWORDS,
        cb->getDoc().data()
    };

    PyObject* cap = PyCapsule_New(const_cast<void*>(reinterpret_cast<const void*>(cb)), "callable", nullptr);
    if (!cap) {
        delete def;
        return nullptr;
    }

    PyObject* fn = PyCFunction_NewEx(def, cap, nullptr);
    if (!fn) {
        Py_DECREF(cap);
        delete def;
        return nullptr;
    }

    if (PyModule_AddObject(module, cb->getName().data(), fn) != 0) {
        Py_DECREF(fn);
        return nullptr;
    }

    return fn;
}

template <typename T>
inline void addFunctionsWithTuple(PyObject* module, const T& tuple) {
    std::apply([module](auto&&... t) { (addFunction(module, &t), ...); }, tuple);
}

[[nodiscard]] inline PyObject* initModule(const char* name) {
    PyModuleDef* defs = new PyModuleDef{PyModuleDef_HEAD_INIT, name, nullptr, -1, nullptr};

    if (auto m = PyModule_Create(defs); m) {
        return m;
    }
    throw std::runtime_error{"failed to create module"};
}

template <typename E>
inline void addEntities(PyObject* m, E&& entities) {
    addFunctionsWithTuple(m, entities.functions);
    std::println("num classes {}", entities.getClasses().size());

    // add classes
    for (auto&& c : entities.getClasses()) {
        auto uc = const_cast<PyTypeObject*>(&c);
        PyType_Ready(uc);
        Py_INCREF(uc);
        std::ignore = PyModule_AddObjectRef(m, c.tp_name, reinterpret_cast<PyObject*>(uc));
    }
}

}  // namespace rebind

#define REFLB_CONCAT_RAW(a, b) a##b
#define REFLB_CONCAT(a, b) REFLB_CONCAT_RAW(a, b)

// Helper macros to create PyInit_ function.
// This macros creates initializing module function and adds functions from
// `entity` to it. Note: Not sure if it's possible to do that via C++ std::meta
// by now. Limitations:
// - no noexcept detection
// - no ref qualifiers
// - no default args
// - and more.

#define REFLB_MODULE(name, entity)                                 \
    PyMODINIT_FUNC REFLB_CONCAT(PyInit_, name)() {                 \
        const char* cname = #name;                                 \
        PyObject* m = rebind::initModule(cname);                   \
        static const auto& entities = rebind::reflect<^^entity>(); \
        addEntities(m, entities);                                  \
        return m;                                                  \
    }
