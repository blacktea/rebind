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

template<size_t Capacity>
inline void addFunctions(PyObject* module, std::inplace_vector<PyMethodDef, Capacity>& functions) {
    for (auto& def : functions) {
        PyObject* fn = PyCFunction_New(&def, nullptr);
        if (!fn) {
            return;
        }

        if (PyModule_AddObjectRef(module, def.ml_name, fn) != 0) {
            Py_DECREF(fn);
            return;
        }
        Py_DECREF(fn);
    }
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
    addFunctions(m, entities.functions);

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
