#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <array>
#include <functional>
#include <meta>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "cast.hpp"
#include "function_traits.hpp"

namespace rebind {

// The method counts number of methods in a given namespace
// Template Type denotes namespace.
template <auto Type>
inline consteval size_t numOfMembers() noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();

    return std::ranges::distance(std::meta::members_of(Type, ctx));
}

// ========================
// Free function reflection
// ========================

class CallableBase {
public:
    PyObject* (*invoke)(const CallableBase*, PyObject* args, PyObject* kwargs);
};

template <typename Fn>
class CallableInfo final : public CallableBase {
    using traits = function_traits<Fn>;
    using return_type = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;

public:
    constexpr explicit CallableInfo(std::string_view fnName, Fn f) noexcept : name{fnName}, fn{f} {
        this->invoke = &this->pyWrapperThunk;
    }

    static PyObject* pyWrapperThunk(const CallableBase* base, PyObject* args, PyObject* kwargs) noexcept {
        return static_cast<const CallableInfo<Fn>*>(base)->pyWrapper(args, kwargs);
    }

    [[nodiscard]] constexpr std::string_view getName() const noexcept { return name; }

    [[nodiscard]] std::string_view getDoc() const noexcept { return doc; }

private:
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

    std::string_view name{};
    std::string_view doc{"doc"};
    Fn fn;
};

template <typename Fn>
CallableInfo(std::string_view, Fn) -> CallableInfo<Fn>;

// The method creates CallableInfo from a function in the Type at I index.
template <auto Type, size_t I>
inline consteval auto getFunction() noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();
    constexpr auto members = std::define_static_array(std::meta::members_of(Type, ctx));

    if constexpr (std::meta::is_function(members[I])) {
        constexpr std::string_view name = std::meta::identifier_of(members[I]);
        return std::make_tuple(CallableInfo{name, [:members[I]:]});
    } else {
        return std::tuple<>();
    }
}

template <std::meta::info Type, size_t... I>
inline consteval auto collectFunctionsImpl(std::index_sequence<I...>) noexcept {
    return std::tuple_cat(getFunction<Type, I>()...);
}

template <std::meta::info Type>
[[nodiscard]] inline consteval auto collectFunctions() noexcept {
    return collectFunctionsImpl<Type>(std::make_index_sequence<numOfMembers<Type>()>{});
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

// =======================
// Struct/Class reflection
// =======================

template <typename T>
struct PyClassWrapper {
    struct Storage;
    consteval {
        std::vector<std::meta::info> mems;
        mems.push_back(
            std::meta::data_member_spec(
                ^^T,
                {
                    .name = "cpp_class"
                }
            )
        );
        std::meta::define_aggregate(^^Storage, mems);
    }
    PyObject_HEAD Storage store{};

    static PyObject* create(PyTypeObject* o, PyObject* args, PyObject* kwds) { return o->tp_alloc(o, 0); }

    static int init(PyObject* op, PyObject* args, PyObject* kwds) {
        // TODO: static_cast<empty<T>*>(op);
        return 0;
    }

    static void dealloc(PyObject* o) { Py_TYPE(o)->tp_free(o); }
};

template <std::meta::info C>
inline consteval PyTypeObject reflect_class() noexcept {
    constexpr std::meta::info t = std::meta::substitute(
        ^^PyClassWrapper,
        {
            C
        }
    );
    const auto class_name = std::meta::identifier_of(C);
    using type_t = typename[:t:];

    return PyTypeObject{
        .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = class_name.data(),  //< TODO: include module name.
        .tp_basicsize = sizeof(type_t),
        .tp_itemsize = 0,
        .tp_dealloc = type_t::dealloc,
        .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
        .tp_doc = PyDoc_STR(class_name.data()),
        .tp_init = type_t::init,
        .tp_new = type_t::create,
    };
}

// =======================
// Reflection
// =======================

template <typename FunctionTuple, size_t CN>
struct Entities {
    FunctionTuple functions;
    std::array<PyTypeObject, CN> classes;
    size_t num_of_classes{};

    constexpr void addClass(PyTypeObject o) { classes[num_of_classes++] = o; }

    constexpr auto getClasses() const {
        return std::ranges::subrange(classes.begin(), classes.begin() + num_of_classes);
    }
};

template <std::meta::info N>
inline consteval auto reflect() {
    constexpr auto functions = collectFunctions<N>();
    Entities<decltype(functions), numOfMembers<N>()> entities{
        .functions = functions,
    };

    static constexpr auto ctx = std::meta::access_context::unprivileged();

    template for (constexpr auto m : std::define_static_array(std::meta::members_of(N, ctx))) {
        if constexpr (std::meta::is_function(m)) {
            continue;
        } else if constexpr (std::meta::is_class_type(m)) {
            entities.addClass(reflect_class<m>());
        }
    }
    return entities;
}

}  // namespace rebind
