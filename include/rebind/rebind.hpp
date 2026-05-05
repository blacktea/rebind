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

extern "C" inline void destroyFunctionCapsule(PyObject* capsule) {
    if (!PyCapsule_IsValid(capsule, "callable")) {
        return;
    }

    delete static_cast<PyMethodDef*>(PyCapsule_GetContext(capsule));
}

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

    PyObject* cap = PyCapsule_New(const_cast<void*>(reinterpret_cast<const void*>(cb)), "callable", destroyFunctionCapsule);
    if (!cap) {
        delete def;
        return nullptr;
    }

    if (PyCapsule_SetContext(cap, def) != 0) {
        Py_DECREF(cap);
        return nullptr;
    }

    PyObject* fn = PyCFunction_NewEx(def, cap, nullptr);
    if (!fn) {
        Py_DECREF(cap);
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
    auto* defs = new PyModuleDef{PyModuleDef_HEAD_INIT, name, nullptr, -1, nullptr};
    if (auto m = PyModule_Create(defs); m) {
        return m;
    }
    delete defs;
    throw std::runtime_error{"failed to create module"};
}

[[nodiscard]] inline PyObject* initModule(PyModuleDef* defs) {
    if (auto m = PyModule_Create(defs); m) {
        return m;
    }
    throw std::runtime_error{"failed to create module"};
}

template <typename E>
inline void addEntities(PyObject* m, E&& entities) {
    addFunctionsWithTuple(m, entities.functions);

    std::apply(
        [m](auto&... class_desc) {
            (
                [&] {
                    class_desc.type.tp_methods = class_desc.methods.data();
                    PyType_Ready(&class_desc.type);
                    Py_INCREF(&class_desc.type);
                    std::ignore = PyModule_AddObjectRef(
                        m,
                        class_desc.type.tp_name,
                        reinterpret_cast<PyObject*>(&class_desc.type)
                    );
                }(),
                ...
            );
        },
        entities.classes
    );
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

#define REFLB_MODULE(name, entity)                          \
    PyMODINIT_FUNC REFLB_CONCAT(PyInit_, name)() {          \
        const char* cname = #name;                          \
        static PyModuleDef defs{PyModuleDef_HEAD_INIT, cname, nullptr, -1, nullptr}; \
        PyObject* m = rebind::initModule(&defs);            \
        static auto entities = rebind::reflect<^^entity>(); \
        rebind::addEntities(m, entities);                   \
        return m;                                           \
    }
