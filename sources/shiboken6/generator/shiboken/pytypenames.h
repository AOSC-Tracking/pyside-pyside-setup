/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt for Python.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef PYTYPENAMES_H
#define PYTYPENAMES_H

#include <QtCore/QString>

static inline QString pyBoolT() { return QStringLiteral("PyBool"); }
static inline QString pyFloatT() { return QStringLiteral("PyFloat"); }
static inline QString pyIntT() { return QStringLiteral("PyInt"); }
static inline QString pyLongT() { return QStringLiteral("PyLong"); }
static inline QString pyObjectT() { return QStringLiteral("object"); }
static inline QString pyStrT() { return QStringLiteral("str"); }

// PYSIDE-1499: A custom type determined by existence of an `__fspath__` attribute.
static inline QString pyPathLikeT() { return QStringLiteral("PyPathLike"); }

static inline QString cPyBufferT() { return QStringLiteral("PyBuffer"); }
static inline QString cPyListT() { return QStringLiteral("PyList"); }
static inline QString cPyObjectT() { return QStringLiteral("PyObject"); }
static inline QString cPySequenceT() { return QStringLiteral("PySequence"); }
static inline QString cPyTypeObjectT() { return QStringLiteral("PyTypeObject"); }

// numpy
static inline QString cPyArrayObjectT() { return QStringLiteral("PyArrayObject"); }

static inline QString sbkCharT() { return QStringLiteral("SbkChar"); }

#endif // PYTYPENAMES_H
