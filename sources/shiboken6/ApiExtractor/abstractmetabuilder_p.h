/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef ABSTRACTMETABUILDER_P_H
#define ABSTRACTMETABUILDER_P_H

#include "abstractmetabuilder.h"
#include "dependency.h"
#include "parser/codemodel_fwd.h"
#include "abstractmetalang.h"
#include "abstractmetatype.h"
#include "include.h"
#include "modifications.h"
#include "typeparser.h"

#include <QSet>
#include <QFileInfo>
#include <QList>

#include <optional>

class TypeDatabase;

class AbstractMetaBuilderPrivate
{
public:
    struct TypeClassEntry
    {
        AbstractMetaType type;
        const AbstractMetaClass *klass;
    };

    using TranslateTypeFlags = AbstractMetaBuilder::TranslateTypeFlags;

    Q_DISABLE_COPY(AbstractMetaBuilderPrivate)

    AbstractMetaBuilderPrivate();
    ~AbstractMetaBuilderPrivate();

    static FileModelItem buildDom(QByteArrayList arguments,
                                  bool addCompilerSupportArguments,
                                  LanguageLevel level,
                                  unsigned clangFlags);
    void traverseDom(const FileModelItem &dom);

    void dumpLog() const;
    static AbstractMetaClassList classesTopologicalSorted(const AbstractMetaClassList &classList,
                                                          const Dependencies &additionalDependencies = {});
    NamespaceModelItem popScope() { return m_scopes.takeLast(); }

    void pushScope(const NamespaceModelItem &item);

    NamespaceModelItem currentScope() const { return m_scopes.constLast(); }

    AbstractMetaClass *argumentToClass(const ArgumentModelItem &,
                                       const AbstractMetaClass *currentClass);

    void addAbstractMetaClass(AbstractMetaClass *cls, const _CodeModelItem *item);
    AbstractMetaClass *traverseTypeDef(const FileModelItem &dom,
                                       const TypeDefModelItem &typeDef,
                                       AbstractMetaClass *currentClass);
    void traverseTypesystemTypedefs();
    AbstractMetaClass *traverseClass(const FileModelItem &dom,
                                     const ClassModelItem &item,
                                     AbstractMetaClass *currentClass);
    void traverseScopeMembers(const ScopeModelItem &item, AbstractMetaClass *metaClass);
    void traverseClassMembers(const ClassModelItem &scopeItem);
    void traverseUsingMembers(AbstractMetaClass *metaClass);
    void traverseNamespaceMembers(const NamespaceModelItem &scopeItem);
    bool setupInheritance(AbstractMetaClass *metaClass);
    AbstractMetaClass *traverseNamespace(const FileModelItem &dom,
                                         const NamespaceModelItem &item);
    std::optional<AbstractMetaEnum> traverseEnum(const EnumModelItem &item,
                                                 AbstractMetaClass *enclosing,
                                                 const QSet<QString> &enumsDeclarations);
    void traverseEnums(const ScopeModelItem &item, AbstractMetaClass *parent,
                       const QStringList &enumsDeclarations);
    AbstractMetaFunctionRawPtrList classFunctionList(const ScopeModelItem &scopeItem,
                                                     AbstractMetaClass::Attributes *constructorAttributes,
                                                     AbstractMetaClass *currentClass);
    void traverseFunctions(ScopeModelItem item, AbstractMetaClass *parent);
    static void applyFunctionModifications(AbstractMetaFunction *func);
    void traverseFields(const ScopeModelItem &item, AbstractMetaClass *parent);
    bool traverseStreamOperator(const FunctionModelItem &functionItem,
                                AbstractMetaClass *currentClass);
    void traverseOperatorFunction(const FunctionModelItem &item,
                                  AbstractMetaClass *currentClass);
    AbstractMetaFunction *traverseAddedFunctionHelper(const AddedFunctionPtr &addedFunc,
                                                      AbstractMetaClass *metaClass,
                                                      QString *errorMessage);
    bool traverseAddedGlobalFunction(const AddedFunctionPtr &addedFunc,
                                     QString *errorMessage);
    bool traverseAddedMemberFunction(const AddedFunctionPtr &addedFunc,
                                     AbstractMetaClass *metaClass,
                                     QString *errorMessage);
    AbstractMetaFunction *traverseFunction(const FunctionModelItem &function,
                                           AbstractMetaClass *currentClass);
    std::optional<AbstractMetaField> traverseField(const VariableModelItem &field,
                                                   const AbstractMetaClass *cls);
    void checkFunctionModifications();
    void registerHashFunction(const FunctionModelItem &functionItem,
                              AbstractMetaClass *currentClass);
    void registerToStringCapabilityIn(const NamespaceModelItem &namespaceItem);
    void registerToStringCapability(const FunctionModelItem &functionItem,
                                    AbstractMetaClass *currentClass);

    /**
     *   A conversion operator function should not have its owner class as
     *   its return type, but unfortunately it does. This function fixes the
     *   return type of operator functions of this kind making the return type
     *   be the same as it is supposed to generate when used in C++.
     *   If the returned type is a wrapped C++ class, this method also adds the
     *   conversion operator to the collection of external conversions of the
     *   said class.
     *   \param metaFunction conversion operator function to be fixed.
     */
    static void fixReturnTypeOfConversionOperator(AbstractMetaFunction *metaFunction);

    void parseQ_Properties(AbstractMetaClass *metaClass, const QStringList &declarations);
    void setupEquals(AbstractMetaClass *metaClass);
    void setupComparable(AbstractMetaClass *metaClass);
    static void setupClonable(AbstractMetaClass *cls);
    void setupExternalConversion(AbstractMetaClass *cls);
    static void setupFunctionDefaults(AbstractMetaFunction *metaFunction,
                                      AbstractMetaClass *metaClass);

    static bool isQualifiedCppIdentifier(QStringView e);
    QString fixDefaultValue(QString expr, const AbstractMetaType &type,
                            const AbstractMetaClass *) const;
    QString fixSimpleDefaultValue(QStringView expr,
                                  const AbstractMetaClass *klass) const;

    QString fixEnumDefault(const AbstractMetaType &type, const QString &expr) const;
    /// Qualify a static field name for default value expressions
    static QString qualifyStaticField(const AbstractMetaClass *c, QStringView field);

    std::optional<AbstractMetaType>
        translateType(const TypeInfo &type, const AbstractMetaClass *currentClass,
                      TranslateTypeFlags flags = {}, QString *errorMessage = nullptr);
    static std::optional<AbstractMetaType>
        translateTypeStatic(const TypeInfo &type, const AbstractMetaClass *current,
                            AbstractMetaBuilderPrivate *d = nullptr, TranslateTypeFlags flags = {},
                            QString *errorMessageIn = nullptr);
    static TypeEntries findTypeEntriesHelper(const QString &qualifiedName, const QString &name,
                                             const AbstractMetaClass *currentClass = nullptr,
                                             AbstractMetaBuilderPrivate *d = nullptr);
    static TypeEntries findTypeEntries(const QString &qualifiedName, const QString &name,
                                       const AbstractMetaClass *currentClass = nullptr,
                                       AbstractMetaBuilderPrivate *d = nullptr,
                                       QString *errorMessage = nullptr);

    qint64 findOutValueFromString(const QString &stringValue, bool &ok);

    AbstractMetaClass *findTemplateClass(const QString& name, const AbstractMetaClass *context,
                                         TypeInfo *info = Q_NULLPTR,
                                         ComplexTypeEntry **baseContainerType = Q_NULLPTR) const;
    AbstractMetaClassList getBaseClasses(const AbstractMetaClass *metaClass) const;

    static bool inheritTemplate(AbstractMetaClass *subclass,
                                const AbstractMetaClass *templateClass,
                                const TypeInfo &info);
    void inheritTemplateFunctions(AbstractMetaClass *subclass);
    std::optional<AbstractMetaType>
        inheritTemplateType(const AbstractMetaTypeList &templateTypes,
                            const AbstractMetaType &metaType);

    static bool isQObject(const FileModelItem &dom, const QString &qualifiedName);
    static bool isEnum(const FileModelItem &dom, const QStringList &qualifiedName);

    void sortLists();
    void setInclude(TypeEntry *te, const QString &path) const;
    static void fixArgumentNames(AbstractMetaFunction *func, const FunctionModificationList &mods);

    void fillAddedFunctions(AbstractMetaClass *metaClass);
    const AbstractMetaClass *resolveTypeSystemTypeDef(const AbstractMetaType &t) const;

    AbstractMetaBuilder *q;
    AbstractMetaClassList m_metaClasses;
    AbstractMetaClassList m_templates;
    AbstractMetaClassList m_smartPointers;
    QHash<const _CodeModelItem *, AbstractMetaClass *> m_itemToClass;
    QHash<const AbstractMetaClass *, const _CodeModelItem *> m_classToItem;
    AbstractMetaFunctionCList m_globalFunctions;
    AbstractMetaEnumList m_globalEnums;

    using RejectMap = QMap<QString, AbstractMetaBuilder::RejectReason>;

    RejectMap m_rejectedClasses;
    RejectMap m_rejectedEnums;
    RejectMap m_rejectedFunctions;
    RejectMap m_rejectedFields;

    QHash<const TypeEntry *, AbstractMetaEnum> m_enums;

    QList<NamespaceModelItem> m_scopes;

    QString m_logDirectory;
    QFileInfoList m_globalHeaders;
    QStringList m_headerPaths;
    mutable QHash<QString, Include> m_resolveIncludeHash;
    QList<TypeClassEntry> m_typeSystemTypeDefs; // look up metatype->class for type system typedefs
    bool m_skipDeprecated = false;
    static bool m_useGlobalHeader;
    static bool m_codeModelTestMode;
};

#endif // ABSTRACTMETBUILDER_P_H
