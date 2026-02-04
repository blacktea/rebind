#pragma once

#include <stdexcept>
#include <string_view>
#include <type_traits>

#include <Python.h>

namespace rebind {

template <typename CType>
PyObject* cast_to_python(CType&& value) {
    using CTypeRaw = std::remove_cvref_t<CType>;

    // Floating points: float, double.
    if constexpr (std::is_floating_point_v<CTypeRaw>) {
        return PyFloat_FromDouble(value);
    }
    // Longs
    if constexpr (std::is_same_v<CTypeRaw, long>) {
        return PyLong_FromLong(value);
    }
    if constexpr (std::is_same_v<CTypeRaw, unsigned long>) {
        return PyLong_FromUnsignedLong(value);
    }
    if constexpr (std::is_same_v<CTypeRaw, long long>) {
        return PyLong_FromLongLong(value);
    }
    if constexpr (std::is_same_v<CTypeRaw, unsigned long long>) {
        return PyLong_FromUnsignedLongLong(value);
    }
    // Boolean. Must be earlier, than `std::is_integral_v`
    if constexpr (std::is_same_v<CTypeRaw, bool>) {
        if (value) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }

    // Integers
    /*
    // Added in Python 3.14
    if constexpr (std::is_same_v<CTypeRaw, int32_t>) {
        return PyLong_FromInt32(value);
    }
    */
    if constexpr (std::is_same_v<CTypeRaw, uint32_t>) {
        return PyLong_FromUInt32(value);
    }
    if constexpr (std::is_same_v<CTypeRaw, int64_t>) {
        return PyLong_FromInt64(value);
    }
    if constexpr (std::is_same_v<CTypeRaw, uint64_t>) {
        return PyLong_FromUInt64(value);
    }
    // size_t
    if constexpr (std::is_same_v<CTypeRaw, size_t>) {
        return PyLong_FromSize_t(value);
    }
    if constexpr (std::is_same_v<CTypeRaw, ssize_t>) {
        return PyLong_FromSsize_t(value);
    }
    // As fallback
    if constexpr (std::is_integral_v<CTypeRaw>) {
        return PyLong_FromLong(value);
    }

    // Strings
    if constexpr (std::is_same_v<CTypeRaw, std::string_view> || std::is_same_v<CTypeRaw, std::string>) {
        return PyUnicode_FromStringAndSize(value.data(), value.size());
    }
    if constexpr (std::is_same_v<CTypeRaw, const char*>) {
        return PyUnicode_FromString(value);
    }
    return nullptr;
}

template <typename CType>
CType cast_to_cpp(PyObject* obj) {
    if (!obj) {
        throw std::runtime_error{"python object must be set"};
    }
    if constexpr (std::is_integral_v<CType>) {
        if (!PyLong_Check(obj)) {
            throw std::runtime_error{"type is not compatible with C++ long"};
        }
        return static_cast<CType>(PyLong_AsLong(obj));
    }
    if constexpr (std::is_floating_point_v<CType>) {
        if (!PyFloat_Check(obj)) {
            throw std::runtime_error{"type is not compatible with C++ floating point"};
        }
        return static_cast<CType>(PyFloat_AsDouble(obj));
    }
    return CType{};
}

template <typename CType>
CType cast_tuple_item_to_cpp(PyObject* args, size_t id) {
    PyObject* arg1{PyTuple_GetItem(args, id)};
    if (!arg1) {
        throw std::runtime_error{"tuple element is null"};
    }
    return cast_to_cpp<CType>(arg1);
}

}  // namespace rebind