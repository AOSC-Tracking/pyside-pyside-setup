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

#ifndef APIEXTRACTORRESULT_H
#define APIEXTRACTORRESULT_H

#include "abstractmetalang.h"
#include "apiextractorflags.h"
#include "abstractmetaenum.h"
#include "abstractmetatype.h"
#include "typesystem_typedefs.h"

#include <QtCore/QHash>
#include <QtCore/QExplicitlySharedDataPointer>

#include <optional>

class ApiExtractorResultData;

struct InstantiatedSmartPointer
{
    const AbstractMetaClass *smartPointer = nullptr;
    AbstractMetaType type;
};

using InstantiatedSmartPointers = QList<InstantiatedSmartPointer>;

/// Result of an ApiExtractor run.
class ApiExtractorResult
{
public:
    ApiExtractorResult();
    explicit ApiExtractorResult(ApiExtractorResultData *data);
    ApiExtractorResult(const ApiExtractorResult &);
    ApiExtractorResult &operator=(const ApiExtractorResult &);
    ApiExtractorResult(ApiExtractorResult &&);
    ApiExtractorResult &operator=(ApiExtractorResult &&);
    ~ApiExtractorResult();

    const AbstractMetaEnumList &globalEnums() const;
    const AbstractMetaFunctionCList &globalFunctions() const;
    const AbstractMetaClassCList &classes() const;
    const AbstractMetaClassCList &smartPointers() const;

    const AbstractMetaTypeList &instantiatedContainers() const;
    const InstantiatedSmartPointers &instantiatedSmartPointers() const;

    // Query functions for the generators
    std::optional<AbstractMetaEnum> findAbstractMetaEnum(const TypeEntry* typeEntry) const;

    /// Retrieves a list of constructors used in implicit conversions
    /// available on the given type. The TypeEntry must be a value-type
    /// or else it will return an empty list.
    ///  \param type a TypeEntry that is expected to be a value-type
    /// \return a list of constructors that could be used as implicit converters
    AbstractMetaFunctionCList implicitConversions(const TypeEntry *type) const;
    AbstractMetaFunctionCList implicitConversions(const AbstractMetaType &metaType) const;

    ApiExtractorFlags flags() const;
    void setFlags(ApiExtractorFlags f);

private:
    QExplicitlySharedDataPointer<ApiExtractorResultData> d;
};

#endif // APIEXTRACTORRESULT_H
