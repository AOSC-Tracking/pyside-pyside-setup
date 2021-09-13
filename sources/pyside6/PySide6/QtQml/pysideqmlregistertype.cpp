/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt for Python.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "pysideqmlregistertype.h"

#include <limits>

// shiboken
#include <shiboken.h>
#include <signature.h>

// pyside
#include <pyside.h>
#include <pyside_p.h>
#include <pysideproperty.h>

// auto generated headers
#include "pyside6_qtcore_python.h"
#include "pyside6_qtqml_python.h"

#include <QtQml/QJSValue>

// Forward declarations.
static void propListMetaCall(PySideProperty *pp, PyObject *self, QMetaObject::Call call,
                             void **args);

// Mutex used to avoid race condition on PySide::nextQObjectMemoryAddr.
static QMutex nextQmlElementMutex;

static void createInto(void *memory, void *type)
{
    QMutexLocker locker(&nextQmlElementMutex);
    PySide::setNextQObjectMemoryAddr(memory);
    Shiboken::GilState state;
    PyObject *obj = PyObject_CallObject(reinterpret_cast<PyObject *>(type), 0);
    if (!obj || PyErr_Occurred())
        PyErr_Print();
    PySide::setNextQObjectMemoryAddr(0);
}

int PySide::qmlRegisterType(PyObject *pyObj, const char *uri, int versionMajor,
                            int versionMinor, const char *qmlName, const char *noCreationReason,
                            bool creatable)
{
    using namespace Shiboken;

    static PyTypeObject *qobjectType = Shiboken::Conversions::getPythonTypeObject("QObject*");
    assert(qobjectType);

    PyTypeObject *pyObjType = reinterpret_cast<PyTypeObject *>(pyObj);
    if (!PySequence_Contains(pyObjType->tp_mro, reinterpret_cast<PyObject *>(qobjectType))) {
        PyErr_Format(PyExc_TypeError, "A type inherited from %s expected, got %s.",
                     qobjectType->tp_name, pyObjType->tp_name);
        return -1;
    }

    const QMetaObject *metaObject = PySide::retrieveMetaObject(pyObjType);
    Q_ASSERT(metaObject);

    QQmlPrivate::RegisterType type;

    // Allow registering Qt Quick items.
    bool registered = false;
#ifdef PYSIDE_QML_SUPPORT
    QuickRegisterItemFunction quickRegisterItemFunction = getQuickRegisterItemFunction();
    if (quickRegisterItemFunction) {
        registered =
            quickRegisterItemFunction(pyObj, uri, versionMajor, versionMinor,
                                      qmlName, creatable, noCreationReason, &type);
    }
#endif

    // Register as simple QObject rather than Qt Quick item.
    if (!registered) {
        // Incref the type object, don't worry about decref'ing it because
        // there's no way to unregister a QML type.
        Py_INCREF(pyObj);

        type.structVersion = 0;

        // FIXME: Fix this to assign new type ids each time.
        type.typeId = QMetaType(QMetaType::QObjectStar);
        type.listId = QMetaType::fromType<QQmlListProperty<QObject> >();
        type.attachedPropertiesFunction = QQmlPrivate::attachedPropertiesFunc<QObject>();
        type.attachedPropertiesMetaObject = QQmlPrivate::attachedPropertiesMetaObject<QObject>();

        type.parserStatusCast =
                QQmlPrivate::StaticCastSelector<QObject, QQmlParserStatus>::cast();
        type.valueSourceCast =
                QQmlPrivate::StaticCastSelector<QObject, QQmlPropertyValueSource>::cast();
        type.valueInterceptorCast =
                QQmlPrivate::StaticCastSelector<QObject, QQmlPropertyValueInterceptor>::cast();

        int objectSize = static_cast<int>(PySide::getSizeOfQObject(
                                              reinterpret_cast<PyTypeObject *>(pyObj)));
        type.objectSize = objectSize;
        type.create = creatable ? createInto : nullptr;
        type.noCreationReason = noCreationReason;
        type.userdata = pyObj;
        type.uri = uri;
        type.version = QTypeRevision::fromVersion(versionMajor, versionMinor);
        type.elementName = qmlName;

        type.extensionObjectCreate = 0;
        type.extensionMetaObject = 0;
        type.customParser = 0;
    }
    type.metaObject = metaObject; // Snapshot may have changed.

    int qmlTypeId = QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
    if (qmlTypeId == -1) {
        PyErr_Format(PyExc_TypeError, "QML meta type registration of \"%s\" failed.",
                     qmlName);
    }
    return qmlTypeId;
}

int PySide::qmlRegisterSingletonType(PyObject *pyObj, const char *uri, int versionMajor,
                                     int versionMinor, const char *qmlName, PyObject *callback,
                                     bool isQObject, bool hasCallback)
{
    using namespace Shiboken;

    if (hasCallback) {
        if (!PyCallable_Check(callback)) {
            PyErr_Format(PyExc_TypeError, "Invalid callback specified.");
            return -1;
        }

        AutoDecRef funcCode(PyObject_GetAttrString(callback, "__code__"));
        AutoDecRef argCount(PyObject_GetAttrString(funcCode, "co_argcount"));

        int count = PyLong_AsLong(argCount);

        if (count != 1) {
            PyErr_Format(PyExc_TypeError, "Callback has a bad parameter count.");
            return -1;
        }

        // Make sure the callback never gets deallocated
        Py_INCREF(callback);
    }

    const QMetaObject *metaObject = nullptr;

    if (isQObject) {
        PyTypeObject *pyObjType = reinterpret_cast<PyTypeObject *>(pyObj);

        if (!isQObjectDerived(pyObjType, true))
            return -1;

        // If we don't have a callback we'll need the pyObj to stay allocated indefinitely
        if (!hasCallback)
            Py_INCREF(pyObj);

        metaObject = PySide::retrieveMetaObject(pyObjType);
        Q_ASSERT(metaObject);
    }

    QQmlPrivate::RegisterSingletonType type;
    type.structVersion = 0;

    type.uri = uri;
    type.version = QTypeRevision::fromVersion(versionMajor, versionMinor);
    type.typeName = qmlName;
    type.instanceMetaObject = metaObject;

    if (isQObject) {
        // FIXME: Fix this to assign new type ids each time.
        type.typeId = QMetaType(QMetaType::QObjectStar);

        type.qObjectApi =
            [callback, pyObj, hasCallback](QQmlEngine *engine, QJSEngine *) -> QObject * {
                Shiboken::GilState gil;
                AutoDecRef args(PyTuple_New(hasCallback ? 1 : 0));

                if (hasCallback) {
                    PyTuple_SET_ITEM(args, 0, Conversions::pointerToPython(
                                     SbkPySide6_QtQmlTypes[SBK_QQMLENGINE_IDX],
                                     engine));
                }

                AutoDecRef retVal(PyObject_CallObject(hasCallback ? callback : pyObj, args));

                PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];

                // Make sure the callback returns something we can convert, else the entire application will crash.
                if (retVal.isNull() ||
                    Conversions::isPythonToCppPointerConvertible(qobjectType, retVal) == nullptr) {
                    PyErr_Format(PyExc_TypeError, "Callback returns invalid value.");
                    return nullptr;
                }

                QObject *obj = nullptr;
                Conversions::pythonToCppPointer(qobjectType, retVal, &obj);

                if (obj != nullptr)
                    Py_INCREF(retVal);

                return obj;
            };
    } else {
        type.scriptApi =
            [callback](QQmlEngine *engine, QJSEngine *) -> QJSValue {
                Shiboken::GilState gil;
                AutoDecRef args(PyTuple_New(1));

                PyTuple_SET_ITEM(args, 0, Conversions::pointerToPython(
                                 SbkPySide6_QtQmlTypes[SBK_QQMLENGINE_IDX],
                                 engine));

                AutoDecRef retVal(PyObject_CallObject(callback, args));

                PyTypeObject *qjsvalueType = SbkPySide6_QtQmlTypes[SBK_QJSVALUE_IDX];

                // Make sure the callback returns something we can convert, else the entire application will crash.
                if (retVal.isNull() ||
                    Conversions::isPythonToCppPointerConvertible(qjsvalueType, retVal) == nullptr) {
                    PyErr_Format(PyExc_TypeError, "Callback returns invalid value.");
                    return QJSValue(QJSValue::UndefinedValue);
                }

                QJSValue *val = nullptr;
                Conversions::pythonToCppPointer(qjsvalueType, retVal, &val);

                Py_INCREF(retVal);

                return *val;
            };
    }

    return QQmlPrivate::qmlregister(QQmlPrivate::SingletonRegistration, &type);
}

int PySide::qmlRegisterSingletonInstance(PyObject *pyObj, const char *uri, int versionMajor,
                                         int versionMinor, const char *qmlName,
                                         PyObject *instanceObject)
{
    using namespace Shiboken;

    static PyTypeObject *qobjectType = Conversions::getPythonTypeObject("QObject*");
    assert(qobjectType);

    // Check if the Python Type inherit from QObject
    PyTypeObject *pyObjType = reinterpret_cast<PyTypeObject *>(pyObj);

    if (!isQObjectDerived(pyObjType, true))
        return -1;

    // Check if the instance object derives from QObject
    PyTypeObject *typeInstanceObject = instanceObject->ob_type;

    if (!isQObjectDerived(typeInstanceObject, true))
        return -1;

    // Convert the instanceObject (PyObject) into a QObject
    QObject *instanceQObject = reinterpret_cast<QObject*>(
            Object::cppPointer(reinterpret_cast<SbkObject*>(instanceObject), qobjectType));

    // Create Singleton Functor to pass the QObject to the Type registration step
    // similarly to the case when we have a callback
    QQmlPrivate::SingletonFunctor registrationFunctor;
    registrationFunctor.m_object = instanceQObject;

    const QMetaObject *metaObject = PySide::retrieveMetaObject(pyObjType);
    Q_ASSERT(metaObject);

    QQmlPrivate::RegisterSingletonType type;
    type.structVersion = 0;

    type.uri = uri;
    type.version = QTypeRevision::fromVersion(versionMajor, versionMinor);
    type.typeName = qmlName;
    type.instanceMetaObject = metaObject;

    // FIXME: Fix this to assign new type ids each time.
    type.typeId = QMetaType(QMetaType::QObjectStar);
    type.qObjectApi = registrationFunctor;


    return QQmlPrivate::qmlregister(QQmlPrivate::SingletonRegistration, &type);
}

extern "C"
{

// This is the user data we store in the property.
struct QmlListProperty
{
    PyTypeObject *type;
    PyObject *append;
    PyObject *count;
    PyObject *at;
    PyObject *clear;
    PyObject *replace;
    PyObject *removeLast;
};

static int propListTpInit(PyObject *self, PyObject *args, PyObject *kwds)
{
    static const char *kwlist[] = {"type", "append", "count", "at", "clear", "replace", "removeLast", 0};
    PySideProperty *pySelf = reinterpret_cast<PySideProperty *>(self);
    QmlListProperty *data = new QmlListProperty;
    memset(data, 0, sizeof(QmlListProperty));

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O|OOOOOO:QtQml.ListProperty", (char **) kwlist,
                                     &data->type,
                                     &data->append,
                                     &data->count,
                                     &data->at,
                                     &data->clear,
                                     &data->replace,
                                     &data->removeLast)) {
        delete data;
        return -1;
    }

    static PyTypeObject *qobjectType = Shiboken::Conversions::getPythonTypeObject("QObject*");
    assert(qobjectType);

    if (!PySequence_Contains(data->type->tp_mro, reinterpret_cast<PyObject *>(qobjectType))) {
        PyErr_Format(PyExc_TypeError, "A type inherited from %s expected, got %s.",
                     qobjectType->tp_name, data->type->tp_name);
        delete data;
        return -1;
    }

    if ((data->append && data->append != Py_None && !PyCallable_Check(data->append)) ||
        (data->count && data->count != Py_None && !PyCallable_Check(data->count)) ||
        (data->at && data->at != Py_None && !PyCallable_Check(data->at)) ||
        (data->clear && data->clear != Py_None && !PyCallable_Check(data->clear)) ||
        (data->replace && data->replace != Py_None && !PyCallable_Check(data->replace)) ||
        (data->removeLast && data->removeLast != Py_None && !PyCallable_Check(data->removeLast))) {
        PyErr_Format(PyExc_TypeError, "Non-callable parameter given");
        delete data;
        return -1;
    }

    PySide::Property::setMetaCallHandler(pySelf, &propListMetaCall);
    PySide::Property::setTypeName(pySelf, "QQmlListProperty<QObject>");
    PySide::Property::setUserData(pySelf, data);

    return 0;
}

void propListTpFree(void *self)
{
    auto pySelf = reinterpret_cast<PySideProperty *>(self);
    delete reinterpret_cast<QmlListProperty *>(PySide::Property::userData(pySelf));
    // calls base type constructor
    Py_TYPE(pySelf)->tp_base->tp_free(self);
}

static PyType_Slot PropertyListType_slots[] = {
    {Py_tp_init, reinterpret_cast<void *>(propListTpInit)},
    {Py_tp_free, reinterpret_cast<void *>(propListTpFree)},
    {Py_tp_dealloc, reinterpret_cast<void *>(Sbk_object_dealloc)},
    {0, nullptr}
};
static PyType_Spec PropertyListType_spec = {
    "2:PySide6.QtQml.ListProperty",
    sizeof(PySideProperty),
    0,
    Py_TPFLAGS_DEFAULT,
    PropertyListType_slots,
};


PyTypeObject *PropertyListTypeF(void)
{
    static PyTypeObject *type = nullptr;
    if (!type) {
        PyObject *bases = Py_BuildValue("(O)", PySidePropertyTypeF());
        type = (PyTypeObject *)SbkType_FromSpecWithBases(&PropertyListType_spec, bases);
        Py_XDECREF(bases);
    }
    return type;
}

} // extern "C"

// Implementation of QQmlListProperty<T>::AppendFunction callback
void propListAppender(QQmlListProperty<QObject> *propList, QObject *item)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(2));
    PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];
    PyTuple_SET_ITEM(args, 0,
                     Shiboken::Conversions::pointerToPython(qobjectType, propList->object));
    PyTuple_SET_ITEM(args, 1,
                     Shiboken::Conversions::pointerToPython(qobjectType, item));

    auto data = reinterpret_cast<QmlListProperty *>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->append, args));

    if (PyErr_Occurred())
        PyErr_Print();
}

// Implementation of QQmlListProperty<T>::CountFunction callback
qsizetype propListCount(QQmlListProperty<QObject> *propList)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(1));
    PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];
    PyTuple_SET_ITEM(args, 0,
                     Shiboken::Conversions::pointerToPython(qobjectType, propList->object));

    auto data = reinterpret_cast<QmlListProperty *>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->count, args));

    // Check return type
    int cppResult = 0;
    PythonToCppFunc pythonToCpp = 0;
    if (PyErr_Occurred())
        PyErr_Print();
    else if ((pythonToCpp = Shiboken::Conversions::isPythonToCppConvertible(Shiboken::Conversions::PrimitiveTypeConverter<qsizetype>(), retVal)))
        pythonToCpp(retVal, &cppResult);
    return cppResult;
}

// Implementation of QQmlListProperty<T>::AtFunction callback
QObject *propListAt(QQmlListProperty<QObject> *propList, qsizetype index)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(2));
    PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];
    PyTuple_SET_ITEM(args, 0,
                     Shiboken::Conversions::pointerToPython(qobjectType, propList->object));
    PyTuple_SET_ITEM(args, 1, Shiboken::Conversions::copyToPython(Shiboken::Conversions::PrimitiveTypeConverter<qsizetype>(), &index));

    auto data = reinterpret_cast<QmlListProperty *>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->at, args));

    QObject *result = 0;
    if (PyErr_Occurred())
        PyErr_Print();
    else if (PyType_IsSubtype(Py_TYPE(retVal), data->type))
        Shiboken::Conversions::pythonToCppPointer(qobjectType, retVal, &result);
    return result;
}

// Implementation of QQmlListProperty<T>::ClearFunction callback
void propListClear(QQmlListProperty<QObject> * propList)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(1));
    PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];
    PyTuple_SET_ITEM(args, 0,
                     Shiboken::Conversions::pointerToPython(qobjectType, propList->object));

    auto data = reinterpret_cast<QmlListProperty *>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->clear, args));

    if (PyErr_Occurred())
        PyErr_Print();
}

// Implementation of QQmlListProperty<T>::ReplaceFunction callback
void propListReplace(QQmlListProperty<QObject> *propList, qsizetype index, QObject *value)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(3));
    PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];
    PyTuple_SET_ITEM(args, 0,
                     Shiboken::Conversions::pointerToPython(qobjectType, propList->object));
    PyTuple_SET_ITEM(args, 1, Shiboken::Conversions::copyToPython(Shiboken::Conversions::PrimitiveTypeConverter<qsizetype>(), &index));
    PyTuple_SET_ITEM(args, 2,
                     Shiboken::Conversions::pointerToPython(qobjectType, value));

    auto data = reinterpret_cast<QmlListProperty *>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->replace, args));

    if (PyErr_Occurred())
        PyErr_Print();
}

// Implementation of QQmlListProperty<T>::RemoveLastFunction callback
void propListRemoveLast(QQmlListProperty<QObject> *propList)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(1));
    PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];
    PyTuple_SET_ITEM(args, 0,
                     Shiboken::Conversions::pointerToPython(qobjectType, propList->object));

    auto data = reinterpret_cast<QmlListProperty *>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->removeLast, args));

    if (PyErr_Occurred())
        PyErr_Print();
}

// qt_metacall specialization for ListProperties
static void propListMetaCall(PySideProperty *pp, PyObject *self, QMetaObject::Call call, void **args)
{
    if (call != QMetaObject::ReadProperty)
        return;

    auto data = reinterpret_cast<QmlListProperty *>(PySide::Property::userData(pp));
    QObject *qobj;
    PyTypeObject *qobjectType = SbkPySide6_QtCoreTypes[SBK_QOBJECT_IDX];
    Shiboken::Conversions::pythonToCppPointer(qobjectType, self, &qobj);
    QQmlListProperty<QObject> declProp(qobj, data,
                                       data->append && data->append != Py_None ? &propListAppender : nullptr,
                                       data->count && data->count != Py_None ? &propListCount : nullptr,
                                       data->at && data->at != Py_None ? &propListAt : nullptr,
                                       data->clear && data->clear != Py_None ? &propListClear : nullptr,
                                       data->replace && data->replace != Py_None ? &propListReplace : nullptr,
                                       data->removeLast && data->removeLast != Py_None ? &propListRemoveLast : nullptr);

    // Copy the data to the memory location requested by the meta call
    void *v = args[0];
    *reinterpret_cast<QQmlListProperty<QObject> *>(v) = declProp;
}

// VolatileBool (volatile bool) type definition.

static PyObject *
QtQml_VolatileBoolObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static const char *kwlist[] = {"x", 0};
    PyObject *x = Py_False;
    long ok;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:bool", const_cast<char **>(kwlist), &x))
        return Q_NULLPTR;
    ok = PyObject_IsTrue(x);
    if (ok < 0)
        return Q_NULLPTR;

    QtQml_VolatileBoolObject *self
            = reinterpret_cast<QtQml_VolatileBoolObject *>(type->tp_alloc(type, 0));

    if (self != nullptr)
        self->flag = new AtomicBool(ok);

    return reinterpret_cast<PyObject *>(self);
}

static void QtQml_VolatileBoolObject_dealloc(PyObject *self)
{
    auto volatileBool = reinterpret_cast<QtQml_VolatileBoolObject *>(self);
    delete volatileBool->flag;
    Sbk_object_dealloc(self);
}

static PyObject *
QtQml_VolatileBoolObject_get(QtQml_VolatileBoolObject *self)
{
    return *self->flag ? Py_True : Py_False;
}

static PyObject *
QtQml_VolatileBoolObject_set(QtQml_VolatileBoolObject *self, PyObject *args)
{
    PyObject *value = Py_False;
    long ok;

    if (!PyArg_ParseTuple(args, "O:bool", &value)) {
        return Q_NULLPTR;
    }

    ok = PyObject_IsTrue(value);
    if (ok < 0) {
        PyErr_SetString(PyExc_TypeError, "Not a boolean value.");
        return Q_NULLPTR;
    }

    *self->flag = ok > 0;

    Py_RETURN_NONE;
}

static PyMethodDef QtQml_VolatileBoolObject_methods[] = {
    {"get", reinterpret_cast<PyCFunction>(QtQml_VolatileBoolObject_get), METH_NOARGS,
     "B.get() -> Bool. Returns the value of the volatile boolean"
    },
    {"set", reinterpret_cast<PyCFunction>(QtQml_VolatileBoolObject_set), METH_VARARGS,
     "B.set(a) -> None. Sets the value of the volatile boolean"
    },
    {nullptr, nullptr, 0, nullptr}  /* Sentinel */
};

static PyObject *
QtQml_VolatileBoolObject_repr(QtQml_VolatileBoolObject *self)
{
    PyObject *s;

    if (*self->flag)
        s = PyBytes_FromFormat("%s(True)",
                                Py_TYPE(self)->tp_name);
    else
        s = PyBytes_FromFormat("%s(False)",
                                Py_TYPE(self)->tp_name);
    Py_XINCREF(s);
    return s;
}

static PyObject *
QtQml_VolatileBoolObject_str(QtQml_VolatileBoolObject *self)
{
    PyObject *s;

    if (*self->flag)
        s = PyBytes_FromFormat("%s(True) -> %p",
                                Py_TYPE(self)->tp_name, self->flag);
    else
        s = PyBytes_FromFormat("%s(False) -> %p",
                                Py_TYPE(self)->tp_name, self->flag);
    Py_XINCREF(s);
    return s;
}

static PyType_Slot QtQml_VolatileBoolType_slots[] = {
    {Py_tp_repr, reinterpret_cast<void *>(QtQml_VolatileBoolObject_repr)},
    {Py_tp_str, reinterpret_cast<void *>(QtQml_VolatileBoolObject_str)},
    {Py_tp_methods, reinterpret_cast<void *>(QtQml_VolatileBoolObject_methods)},
    {Py_tp_new, reinterpret_cast<void *>(QtQml_VolatileBoolObject_new)},
    {Py_tp_dealloc, reinterpret_cast<void *>(QtQml_VolatileBoolObject_dealloc)},
    {0, 0}
};
static PyType_Spec QtQml_VolatileBoolType_spec = {
    "2:PySide6.QtQml.VolatileBool",
    sizeof(QtQml_VolatileBoolObject),
    0,
    Py_TPFLAGS_DEFAULT,
    QtQml_VolatileBoolType_slots,
};


PyTypeObject *QtQml_VolatileBoolTypeF(void)
{
    static PyTypeObject *type = reinterpret_cast<PyTypeObject *>(
        SbkType_FromSpec(&QtQml_VolatileBoolType_spec));
    return type;
}

static const char *PropertyList_SignatureStrings[] = {
    "PySide6.QtQml.ListProperty(self,type:type,append:typing.Callable,"
        "at:typing.Callable=None,clear:typing.Callable=None,count:typing.Callable=None)",
    nullptr}; // Sentinel

static const char *VolatileBool_SignatureStrings[] = {
    "PySide6.QtQml.VolatileBool.get(self)->bool",
    "PySide6.QtQml.VolatileBool.set(self,a:object)",
    nullptr}; // Sentinel

void PySide::initQmlSupport(PyObject *module)
{
    // Export QmlListProperty type
    if (InitSignatureStrings(PropertyListTypeF(), PropertyList_SignatureStrings) < 0) {
        PyErr_Print();
        qWarning() << "Error initializing PropertyList type.";
        return;
    }

    // Register QQmlListProperty metatype for use in QML
    qRegisterMetaType<QQmlListProperty<QObject>>();

    Py_INCREF(reinterpret_cast<PyObject *>(PropertyListTypeF()));
    PyModule_AddObject(module, PepType_GetNameStr(PropertyListTypeF()),
                       reinterpret_cast<PyObject *>(PropertyListTypeF()));

    if (InitSignatureStrings(QtQml_VolatileBoolTypeF(), VolatileBool_SignatureStrings) < 0) {
        PyErr_Print();
        qWarning() << "Error initializing VolatileBool type.";
        return;
    }

    Py_INCREF(QtQml_VolatileBoolTypeF());
    PyModule_AddObject(module, PepType_GetNameStr(QtQml_VolatileBoolTypeF()),
                       reinterpret_cast<PyObject *>(QtQml_VolatileBoolTypeF()));
}

static std::string getGlobalString(const char *name)
{
    using Shiboken::AutoDecRef;

    PyObject *globals = PyEval_GetGlobals();

    AutoDecRef pyName(Py_BuildValue("s", name));

    PyObject *globalVar = PyDict_GetItem(globals, pyName);

    if (globalVar == nullptr || !PyUnicode_Check(globalVar))
        return "";

    const char *stringValue = _PepUnicode_AsString(globalVar);
    return stringValue != nullptr ? stringValue : "";
}

static int getGlobalInt(const char *name)
{
    using Shiboken::AutoDecRef;

    PyObject *globals = PyEval_GetGlobals();

    AutoDecRef pyName(Py_BuildValue("s", name));

    PyObject *globalVar = PyDict_GetItem(globals, pyName);

    if (globalVar == nullptr || !PyLong_Check(globalVar))
        return -1;

    long value = PyLong_AsLong(globalVar);

    if (value > std::numeric_limits<int>::max() || value < std::numeric_limits<int>::min())
        return -1;

    return value;
}

PyObject *PySide::qmlElementMacro(PyObject *pyObj)
{
    if (!PyType_Check(pyObj)) {
        PyErr_Format(PyExc_TypeError, "This decorator can only be used on classes.");
        return nullptr;
    }

    static PyTypeObject *qobjectType = Shiboken::Conversions::getPythonTypeObject("QObject*");
    assert(qobjectType);

    PyTypeObject *pyObjType = reinterpret_cast<PyTypeObject *>(pyObj);
    if (!PySequence_Contains(pyObjType->tp_mro, reinterpret_cast<PyObject *>(qobjectType))) {
        PyErr_Format(PyExc_TypeError, "This decorator can only be used with classes inherited from QObject, got %s.", pyObjType->tp_name);
        return nullptr;
    }

    std::string importName = getGlobalString("QML_IMPORT_NAME");
    int majorVersion = getGlobalInt("QML_IMPORT_MAJOR_VERSION");
    int minorVersion = getGlobalInt("QML_IMPORT_MINOR_VERSION");

    if (importName.empty()) {
        PyErr_Format(PyExc_TypeError, "You need specify QML_IMPORT_NAME in order to use QmlElement.");
        return nullptr;
    }

    if (majorVersion == -1) {
       PyErr_Format(PyExc_TypeError, "You need specify QML_IMPORT_MAJOR_VERSION in order to use QmlElement.");
       return nullptr;
    }

    // Specifying a minor version is optional
    if (minorVersion == -1)
        minorVersion = 0;

    if (qmlRegisterType(pyObj, importName.c_str(), majorVersion, minorVersion, pyObjType->tp_name) == -1) {
       PyErr_Format(PyExc_TypeError, "Failed to register type %s.", pyObjType->tp_name);
    }

    return pyObj;
}
