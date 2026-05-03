#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <tuple>
#include <type_traits>
#include <utility>

#include "cast.hpp"
#include "function_traits.hpp"

namespace rebind {

template <typename ArgsTuple, typename Return, typename Invoke>
PyObject* invokePythonCallable(PyObject* args, Invoke&& invoke) noexcept {
    /*
    TODO: Handle errors, exceptions, etc.
    TODO: parse arguments. Currently supports method with no arguments.
    - i: Integer (converts to C int).
    - s: String (converts to const char *).
    - f: Float (converts to C float).
    - d: Double (converts to C double).
    - O: Object (extracts the raw PyObject* without conversion).
    - |: Indicates that subsequent arguments are optional.
    */

    if (!PyTuple_Check(args)) {
        PyErr_SetString(PyExc_ValueError, "Args must be a tuple type");
        Py_RETURN_NONE;
    }

    constexpr size_t arity = std::tuple_size_v<ArgsTuple>;
    if (PyTuple_GET_SIZE(args) != arity) {
        PyErr_SetString(PyExc_ValueError, "Mismatch number of arguments");
        Py_RETURN_NONE;
    }

    auto invoke_with_args = [&]<size_t... I>(std::index_sequence<I...>) -> decltype(auto) {
        return std::forward<Invoke>(invoke)(cast_tuple_item_to_cpp<std::tuple_element_t<I, ArgsTuple>>(args, I)...);
    };

    if constexpr (std::is_void_v<Return>) {
        invoke_with_args(std::make_index_sequence<arity>{});
        Py_RETURN_NONE;
    } else {
        auto result = invoke_with_args(std::make_index_sequence<arity>{});
        PyObject* py_result = cast_to_python(result);
        if (!py_result) {
            PyErr_SetString(PyExc_ValueError, "failed to cast to python object");
            Py_RETURN_NONE;
        }
        return py_result;
    }
}

template <auto Method, typename Wrapper>
struct MethodInvoker {
    using traits = function_traits<decltype(Method)>;
    using return_type = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;

    static PyObject* invoke(PyObject* self, PyObject* args) noexcept {
        auto* wrapper = reinterpret_cast<Wrapper*>(self);
        return invokePythonCallable<args_tuple, return_type>(args, [wrapper](auto&&... converted_args) -> decltype(auto) {
            return (wrapper->store.cpp_class.*Method)(std::forward<decltype(converted_args)>(converted_args)...);
        });
    }
};

}  // namespace rebind
