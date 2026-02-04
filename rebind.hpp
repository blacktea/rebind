#pragma once

#include "cast.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <meta>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <Python.h>

namespace rebind {

// The method counts number of methods in a given namespace
// Template Type denotes namespace.
template <auto Type>
inline consteval size_t numOfMembers() {
    static constexpr auto ctx = std::meta::access_context::unprivileged();

    return std::ranges::distance(std::meta::members_of(Type, ctx));
}

// Helper function to extract return type, arguments and its types of the
// function.
// TODO: use from std::meta. I didn't find any helpful method out there.
template <class T>
struct function_traits;

// 1) Plain function type: R(Args...)
template <class R, class... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;

    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    using arg = std::tuple_element_t<N, args_tuple>;
};

// 2) Function pointer: R(*)(Args...)
template <class R, class... Args>
struct function_traits<R (*)(Args...)> : function_traits<R(Args...)> {};

// 3) Function reference: R(&)(Args...)
template <class R, class... Args>
struct function_traits<R (&)(Args...)> : function_traits<R(Args...)> {};

// The class implements type erasure pattern hiding function signature.
class CallableBase {
public:
    PyObject* (*invoke)(const CallableBase*, PyObject* args, PyObject* kwargs);
};

// The class holds pointer to a function, name, and other information.
template <typename Fn>
class CallableInfo final : public CallableBase {
    using traits = function_traits<Fn>;
    using return_type = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;

public:
    constexpr explicit CallableInfo(const char* fnName, Fn f) noexcept : name{fnName}, fn{f} {
        this->invoke = &this->pyWrapperThunk;
    }

    static PyObject* pyWrapperThunk(const CallableBase* base, PyObject* args, PyObject* kwargs) noexcept {
        return static_cast<const CallableInfo<Fn>*>(base)->pyWrapper(args, kwargs);
    }

    // This function does:
    // - parsing args passed from Python.
    // - invoke C++ method.
    // - return back a result.
    PyObject* pyWrapper(PyObject* args, PyObject* /*kwargs*/) const noexcept {
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

        // Check args is tuple.
        if (!PyTuple_Check(args)) {
            PyErr_SetString(PyExc_ValueError, "Args must be a tuple type");
            Py_RETURN_NONE;
        }

        if (PyTuple_GET_SIZE(args) != std::tuple_size_v<args_tuple>) {
            PyErr_SetString(PyExc_ValueError, "Mismatch number of arguments");
            Py_RETURN_NONE;
        }

        auto invoke_with_args = [this, args]<size_t... I>(std::index_sequence<I...>) {
            if constexpr (std::tuple_size_v<args_tuple> == 0) {
                static_assert(std::tuple_size_v<args_tuple> == 0);
                return this->fn();

            } else {
                static_assert(std::tuple_size_v<args_tuple> > 0);
                return std::invoke(
                    [this, args](auto&&... a) { return this->fn(std::forward<decltype(a)>(a)...); },
                    cast_tuple_item_to_cpp<std::tuple_element_t<I, args_tuple>>(args, I)...
                );
            }
        };

        if constexpr (std::is_void_v<return_type>) {
            invoke_with_args(std::make_index_sequence<0>{});
            Py_RETURN_NONE;
        } else {
            // TODO: support refs, other types(pair, tuples, vectors, etc.).

            auto result = invoke_with_args(std::make_index_sequence<std::tuple_size_v<args_tuple>>{});
            PyObject* py_result = cast_to_python(result);
            if (!py_result) {
                PyErr_SetString(PyExc_ValueError, "failed to cast to python object");
                Py_RETURN_NONE;
            }
            return py_result;
        }
        Py_RETURN_NONE;
    }

    const char* getName() const noexcept {
        assert(name);
        return name;
    }

    const char* getDoc() const noexcept {
        assert(doc);
        return doc;
    }

private:
    const char* name{nullptr};
    const char* doc{"doc"};
    Fn fn;
};

// The method creates Callable info from a function in the Type at I index.
template <auto Type, size_t I>
inline consteval auto getMethod() noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();
    constexpr auto members = std::define_static_array(std::meta::members_of(Type, ctx));
    if constexpr (std::meta::is_function(members[I])) {
        // TODO: NOTE: identifier_of is not null terminated.
        constexpr auto name = std::meta::identifier_of(members[I]);
        return std::make_tuple(CallableInfo{name.data(), [:members[I]:]});
    } else {
        return std::tuple<>();
    }
}

// Utility method that creates tuple of CallableInfo objects.
template <auto Type, size_t... I>
inline consteval auto collectFunctionsImpl(std::index_sequence<I...> seq) noexcept {
    return std::tuple_cat(getMethod<Type, I>()...);
}

// The method gathers functions in the Type(should be namespace) to tuple.
// For example, this method finds `foo` function and create a tuple with it.
// namespace v1 {
// void foo();
// }
template <auto Type>
[[nodiscard]] inline consteval auto collectFunctions() noexcept {
    return collectFunctionsImpl<Type>(std::make_index_sequence<numOfMembers<Type>()>{});
}

template <auto Type>
inline constinit auto functionsStorage = collectFunctions<Type>();

// Tranpoline, addFunction methods are used to be able to invoke free C++
// functions as callable object.
extern "C" inline PyObject* trampoline(PyObject* self, PyObject* args, PyObject* kwargs) {
    const auto* cb = static_cast<const CallableBase*>(PyCapsule_GetPointer(self, "callable"));
    if (!cb) {
        return nullptr;
    }

    return cb->invoke(cb, args, kwargs);
}

template <typename Fn>
inline PyObject* addFunction(PyObject* module, const CallableInfo<Fn>* cb) {
    // NOTE: PyMethodDef must live for the lifetime of the module.
    // We intentionally leak it.
    auto* def = new PyMethodDef{cb->getName(), (PyCFunction)trampoline, METH_VARARGS | METH_KEYWORDS, cb->getDoc()};

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

    if (PyModule_AddObject(module, cb->getName(), fn) != 0) {
        Py_DECREF(fn);  // also decref cap via function internals
        return nullptr;
    }

    // Keep capsule alive by attaching it to the function object
    // (PyCFunction holds a reference to `self` already), so nothing else needed.
    return fn;
}

template <typename T>
inline void addFunctionsWithTuple(PyObject* module, const T& tuple) {
    std::apply(
        [module](auto&&... t) {
            // TODO: handle errors(nullptr-s);
            (addFunction(module, &t), ...);
        },
        tuple
    );
}

[[nodiscard]] inline PyObject* initModule(const char* name) {
    PyModuleDef* defs = new PyModuleDef{PyModuleDef_HEAD_INIT, name, nullptr, -1, nullptr};

    if (auto m = PyModule_Create(defs); m) {
        return m;
    }
    throw std::runtime_error{"failed to create module"};
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

#define REFLB_MODULE(name, entity)                                         \
    PyMODINIT_FUNC REFLB_CONCAT(PyInit_, name)() {                         \
        const char* cname = #name;                                         \
        PyObject* m = rebind::initModule(cname);                           \
        static const auto& functions = rebind::functionsStorage<^^entity>; \
        rebind::addFunctionsWithTuple(m, functions);                       \
        return m;                                                          \
    }
