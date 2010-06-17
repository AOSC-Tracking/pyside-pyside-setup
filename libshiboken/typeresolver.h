/*
 * This file is part of the Shiboken Python Bindings Generator project.
 *
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: PySide team <contact@pyside.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation. Please
 * review the following information to ensure the GNU Lesser General
 * Public License version 2.1 requirements will be met:
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 *
 * As a special exception to the GNU Lesser General Public License
 * version 2.1, the object code form of a "work that uses the Library"
 * may incorporate material from a header file that is part of the
 * Library.  You may distribute such object code under terms of your
 * choice, provided that the incorporated material (i) does not exceed
 * more than 5% of the total size of the Library; and (ii) is limited to
 * numerical parameters, data structure layouts, accessors, macros,
 * inline functions and templates.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef TYPERESOLVER_H
#define TYPERESOLVER_H

#include "shibokenmacros.h"
#include "conversions.h"

namespace Shiboken
{

class SbkBaseWrapperType;

/* To C++ convertion functions. */
template <typename T>
inline void* pythonToValueType(PyObject* pyobj)
{
    return Shiboken::CppObjectCopier<T>::copy(Shiboken::Converter<T>::toCpp(pyobj));
}

template <typename T>
inline void* pythonToObjectType(PyObject* pyobj)
{
    return Shiboken::Converter<T*>::toCpp(pyobj);
}

template <typename T>
inline void objectDeleter(void* data)
{
    delete reinterpret_cast<T*>(data);
}

template <typename T>
inline PyObject* objectTypeToPython(void* cptr)
{
    return Shiboken::Converter<T*>::toPython(*reinterpret_cast<T**>(cptr));
}

/**
*   \internal This function is not part of the public API.
*   Initialize the TypeResource internal cache.
*/
void initTypeResolver();

class LIBSHIBOKEN_API TypeResolver
{
public:
    enum Type
    {
        ObjectType,
        ValueType,
        UnknownType
    };

    typedef PyObject* (*CppToPythonFunc)(void*);
    typedef void* (*PythonToCppFunc)(PyObject*);
    typedef void (*DeleteObjectFunc)(void*);
    typedef PyTypeObject* (*GetPyTypeFunc)();

    ~TypeResolver();

    template<typename T>
    static TypeResolver* createValueTypeResolver(const char* typeName)
    {
        return new TypeResolver(typeName, &Shiboken::Converter<T>::toPython, &pythonToValueType<T>, SbkType<T>(), &objectDeleter<T>);
    }

    template<typename T>
    static TypeResolver* createObjectTypeResolver(const char* typeName)
    {
        return new TypeResolver(typeName, &objectTypeToPython<T>, &pythonToObjectType<T>, SbkType<T>());
    }

    static Type getType(const char* name);
    static TypeResolver* get(const char* typeName);

    const char* typeName() const;
    PyObject* toPython(void* cppObj);
    void* toCpp(PyObject* pyObj);
    void deleteObject(void* object);
    PyTypeObject* pythonType();

private:
    struct TypeResolverPrivate;
    TypeResolverPrivate* m_d;

    // disable object copy
    TypeResolver(const TypeResolver&);
    TypeResolver& operator=(const TypeResolver&);

    TypeResolver(const char* typeName, CppToPythonFunc cppToPy, PythonToCppFunc pyToCpp, PyTypeObject* pyType, DeleteObjectFunc deleter = 0);
};
}

#endif
