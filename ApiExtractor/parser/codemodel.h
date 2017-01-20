/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2002-2005 Roberto Raggi <roberto@kdevelop.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of PySide2.
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


#ifndef CODEMODEL_H
#define CODEMODEL_H

#include "codemodel_fwd.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>

#define DECLARE_MODEL_NODE(k) \
    enum { __node_kind = Kind_##k };

class CodeModel
{
public:
    enum AccessPolicy {
        Public,
        Protected,
        Private
    };

    enum FunctionType {
        Normal,
        Signal,
        Slot
    };

    enum ClassType {
        Class,
        Struct,
        Union
    };

public:
    CodeModel();
    virtual ~CodeModel();

    FileList files() const;
    NamespaceModelItem globalNamespace() const;

    void addFile(FileModelItem item);
    void removeFile(FileModelItem item);
    FileModelItem findFile(const QString &name) const;
    QHash<QString, FileModelItem> fileMap() const;

    CodeModelItem findItem(const QStringList &qualifiedName, CodeModelItem scope) const;

    void wipeout();

private:
    QHash<QString, FileModelItem> _M_files;
    NamespaceModelItem _M_globalNamespace;

private:
    CodeModel(const CodeModel &other);
    void operator = (const CodeModel &other);
};

class TypeInfo
{
public:
    TypeInfo() : flags(0) {}

    QStringList qualifiedName() const
    {
        return m_qualifiedName;
    }

    void setQualifiedName(const QStringList &qualified_name)
    {
        m_qualifiedName = qualified_name;
    }

    bool isConstant() const
    {
        return m_constant;
    }

    void setConstant(bool is)
    {
        m_constant = is;
    }

    bool isVolatile() const
    {
        return m_volatile;
    }

    void setVolatile(bool is)
    {
        m_volatile = is;
    }

    bool isReference() const
    {
        return m_reference;
    }

    void setReference(bool is)
    {
        m_reference = is;
    }

    int indirections() const
    {
        return m_indirections;
    }

    void setIndirections(int indirections)
    {
        m_indirections = indirections;
    }

    bool isFunctionPointer() const
    {
        return m_functionPointer;
    }
    void setFunctionPointer(bool is)
    {
        m_functionPointer = is;
    }

    QStringList arrayElements() const
    {
        return m_arrayElements;
    }
    void setArrayElements(const QStringList &arrayElements)
    {
        m_arrayElements = arrayElements;
    }

    QList<TypeInfo> arguments() const
    {
        return m_arguments;
    }

    void setArguments(const QList<TypeInfo> &arguments);

    void addArgument(const TypeInfo &arg)
    {
        m_arguments.append(arg);
    }

    bool operator==(const TypeInfo &other);

    bool operator!=(const TypeInfo &other)
    {
        return !(*this == other);
    }

    // ### arrays and templates??

    QString toString() const;

    static TypeInfo combine(const TypeInfo &__lhs, const TypeInfo &__rhs);
    static TypeInfo resolveType(TypeInfo const &__type, CodeModelItem __scope);

private:
    static TypeInfo resolveType(CodeModelItem item, TypeInfo const &__type, CodeModelItem __scope);

    QStringList m_qualifiedName;
    QStringList m_arrayElements;
    QList<TypeInfo> m_arguments;

    union {
        uint flags;

        struct {
            uint m_constant: 1;
            uint m_volatile: 1;
            uint m_reference: 1;
            uint m_functionPointer: 1;
            uint m_indirections: 6;
            uint m_padding: 22;
        };
    };
};

class _CodeModelItem
{
    Q_DISABLE_COPY(_CodeModelItem)
public:
    enum Kind {
        /* These are bit-flags resembling inheritance */
        Kind_Scope = 0x1,
        Kind_Namespace = 0x2 | Kind_Scope,
        Kind_Member = 0x4,
        Kind_Function = 0x8 | Kind_Member,
        KindMask = 0xf,

        /* These are for classes that are not inherited from */
        FirstKind = 0x8,
        Kind_Argument = 1 << FirstKind,
        Kind_Class = 2 << FirstKind | Kind_Scope,
        Kind_Enum = 3 << FirstKind,
        Kind_Enumerator = 4 << FirstKind,
        Kind_File = 5 << FirstKind | Kind_Namespace,
        Kind_FunctionDefinition = 6 << FirstKind | Kind_Function,
        Kind_TemplateParameter = 7 << FirstKind,
        Kind_TypeAlias = 8 << FirstKind,
        Kind_Variable = 9 << FirstKind | Kind_Member
    };

public:
    virtual ~_CodeModelItem();

    int kind() const;

    QStringList qualifiedName() const;

    QString name() const;
    void setName(const QString &name);

    QStringList scope() const;
    void setScope(const QStringList &scope);

    QString fileName() const;
    void setFileName(const QString &fileName);

    FileModelItem file() const;

    void getStartPosition(int *line, int *column);
    void setStartPosition(int line, int column);

    void getEndPosition(int *line, int *column);
    void setEndPosition(int line, int column);

    inline CodeModel *model() const
    {
        return _M_model;
    }

protected:
    explicit _CodeModelItem(CodeModel *model, int kind);
    explicit _CodeModelItem(CodeModel *model, const QString &name, int kind);
    void setKind(int kind);

private:
    CodeModel *_M_model;
    int _M_kind;
    int _M_startLine;
    int _M_startColumn;
    int _M_endLine;
    int _M_endColumn;
    std::size_t _M_creation_id;
    QString _M_name;
    QString _M_fileName;
    QStringList _M_scope;
};

class _ScopeModelItem: public _CodeModelItem
{
public:
    DECLARE_MODEL_NODE(Scope)

    ~_ScopeModelItem();

    ClassList classes() const;
    EnumList enums() const;
    FunctionDefinitionList functionDefinitions() const;
    FunctionList functions() const;
    TypeAliasList typeAliases() const;
    VariableList variables() const;

    void addClass(ClassModelItem item);
    void addEnum(EnumModelItem item);
    void addFunction(FunctionModelItem item);
    void addFunctionDefinition(FunctionDefinitionModelItem item);
    void addTypeAlias(TypeAliasModelItem item);
    void addVariable(VariableModelItem item);

    void removeClass(ClassModelItem item);
    void removeEnum(EnumModelItem item);
    void removeFunction(FunctionModelItem item);
    void removeFunctionDefinition(FunctionDefinitionModelItem item);
    void removeTypeAlias(TypeAliasModelItem item);
    void removeVariable(VariableModelItem item);

    ClassModelItem findClass(const QString &name) const;
    EnumModelItem findEnum(const QString &name) const;
    FunctionDefinitionList findFunctionDefinitions(const QString &name) const;
    FunctionList findFunctions(const QString &name) const;
    TypeAliasModelItem findTypeAlias(const QString &name) const;
    VariableModelItem findVariable(const QString &name) const;

    void addEnumsDeclaration(const QString &enumsDeclaration);
    QStringList enumsDeclarations() const
    {
        return _M_enumsDeclarations;
    }

    inline QHash<QString, ClassModelItem> classMap() const
    {
        return _M_classes;
    }
    inline QHash<QString, EnumModelItem> enumMap() const
    {
        return _M_enums;
    }
    inline QHash<QString, TypeAliasModelItem> typeAliasMap() const
    {
        return _M_typeAliases;
    }
    inline QHash<QString, VariableModelItem> variableMap() const
    {
        return _M_variables;
    }
    inline QMultiHash<QString, FunctionDefinitionModelItem> functionDefinitionMap() const
    {
        return _M_functionDefinitions;
    }
    inline QMultiHash<QString, FunctionModelItem> functionMap() const
    {
        return _M_functions;
    }

    FunctionModelItem declaredFunction(FunctionModelItem item);

protected:
    explicit _ScopeModelItem(CodeModel *model, int kind = __node_kind)
        : _CodeModelItem(model, kind) {}
    explicit _ScopeModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _CodeModelItem(model, name, kind) {}

private:
    QHash<QString, ClassModelItem> _M_classes;
    QHash<QString, EnumModelItem> _M_enums;
    QHash<QString, TypeAliasModelItem> _M_typeAliases;
    QHash<QString, VariableModelItem> _M_variables;
    QMultiHash<QString, FunctionDefinitionModelItem> _M_functionDefinitions;
    QMultiHash<QString, FunctionModelItem> _M_functions;

private:
    QStringList _M_enumNames;
    QStringList _M_enumsDeclarations;
};

class _ClassModelItem: public _ScopeModelItem
{
public:
    DECLARE_MODEL_NODE(Class)

    explicit _ClassModelItem(CodeModel *model, int kind = __node_kind)
        : _ScopeModelItem(model, kind), _M_classType(CodeModel::Class) {}
    explicit _ClassModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _ScopeModelItem(model, name, kind), _M_classType(CodeModel::Class) {}
    ~_ClassModelItem();

    QStringList baseClasses() const;

    void setBaseClasses(const QStringList &baseClasses);
    void addBaseClass(const QString &baseClass);
    void removeBaseClass(const QString &baseClass);

    TemplateParameterList templateParameters() const;
    void setTemplateParameters(const TemplateParameterList &templateParameters);

    bool extendsClass(const QString &name) const;

    void setClassType(CodeModel::ClassType type);
    CodeModel::ClassType classType() const;

    void addPropertyDeclaration(const QString &propertyDeclaration);
    QStringList propertyDeclarations() const
    {
        return _M_propertyDeclarations;
    }

private:
    QStringList _M_baseClasses;
    TemplateParameterList _M_templateParameters;
    CodeModel::ClassType _M_classType;

    QStringList _M_propertyDeclarations;
};

class _NamespaceModelItem: public _ScopeModelItem
{
public:
    DECLARE_MODEL_NODE(Namespace)

    explicit _NamespaceModelItem(CodeModel *model, int kind = __node_kind)
        : _ScopeModelItem(model, kind) {}
    explicit _NamespaceModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _ScopeModelItem(model, name, kind) {}
    ~_NamespaceModelItem();

    NamespaceList namespaces() const;

    void addNamespace(NamespaceModelItem item);
    void removeNamespace(NamespaceModelItem item);

    NamespaceModelItem findNamespace(const QString &name) const;

    inline QHash<QString, NamespaceModelItem> namespaceMap() const
    {
        return _M_namespaces;
    };

private:
    QHash<QString, NamespaceModelItem> _M_namespaces;
};

class _FileModelItem: public _NamespaceModelItem
{
public:
    DECLARE_MODEL_NODE(File)

    explicit _FileModelItem(CodeModel *model, int kind = __node_kind)
        : _NamespaceModelItem(model, kind) {}
    explicit _FileModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _NamespaceModelItem(model, name, kind) {}
    ~_FileModelItem();

private:
    _FileModelItem(const _FileModelItem &other);
    void operator = (const _FileModelItem &other);
};

class _ArgumentModelItem: public _CodeModelItem
{
public:
    DECLARE_MODEL_NODE(Argument)

    explicit _ArgumentModelItem(CodeModel *model, int kind = __node_kind)
        : _CodeModelItem(model, kind), _M_defaultValue(false) {}
    explicit _ArgumentModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _CodeModelItem(model, name, kind), _M_defaultValue(false) {}
    ~_ArgumentModelItem();

    TypeInfo type() const;
    void setType(const TypeInfo &type);

    bool defaultValue() const;
    void setDefaultValue(bool defaultValue);

    QString defaultValueExpression() const
    {
        return _M_defaultValueExpression;
    }

    void setDefaultValueExpression(const QString &expr)
    {
        _M_defaultValueExpression = expr;
    }

private:
    TypeInfo _M_type;
    QString _M_defaultValueExpression;
    bool _M_defaultValue;
};

class _MemberModelItem: public _CodeModelItem
{
public:
    DECLARE_MODEL_NODE(Member)

    explicit _MemberModelItem(CodeModel *model, int kind = __node_kind)
        : _CodeModelItem(model, kind), _M_accessPolicy(CodeModel::Public), _M_flags(0) {}
    explicit _MemberModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _CodeModelItem(model, name, kind), _M_accessPolicy(CodeModel::Public), _M_flags(0) {}
    ~_MemberModelItem();

    bool isConstant() const;
    void setConstant(bool isConstant);

    bool isVolatile() const;
    void setVolatile(bool isVolatile);

    bool isStatic() const;
    void setStatic(bool isStatic);

    bool isAuto() const;
    void setAuto(bool isAuto);

    bool isFriend() const;
    void setFriend(bool isFriend);

    bool isRegister() const;
    void setRegister(bool isRegister);

    bool isExtern() const;
    void setExtern(bool isExtern);

    bool isMutable() const;
    void setMutable(bool isMutable);

    CodeModel::AccessPolicy accessPolicy() const;
    void setAccessPolicy(CodeModel::AccessPolicy accessPolicy);

    TemplateParameterList templateParameters() const
    {
        return _M_templateParameters;
    }

    void setTemplateParameters(const TemplateParameterList &templateParameters)
    {
        _M_templateParameters = templateParameters;
    }

    TypeInfo type() const;
    void setType(const TypeInfo &type);

private:
    TemplateParameterList _M_templateParameters;
    TypeInfo _M_type;
    CodeModel::AccessPolicy _M_accessPolicy;
    union {
        struct {
            uint _M_isConstant: 1;
            uint _M_isVolatile: 1;
            uint _M_isStatic: 1;
            uint _M_isAuto: 1;
            uint _M_isFriend: 1;
            uint _M_isRegister: 1;
            uint _M_isExtern: 1;
            uint _M_isMutable: 1;
        };
        uint _M_flags;
    };

};

class _FunctionModelItem: public _MemberModelItem
{
public:
    DECLARE_MODEL_NODE(Function)

    explicit _FunctionModelItem(CodeModel *model, int kind = __node_kind)
        : _MemberModelItem(model, kind), _M_functionType(CodeModel::Normal), _M_flags(0) {}
    explicit _FunctionModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _MemberModelItem(model, name, kind), _M_functionType(CodeModel::Normal), _M_flags(0) {}
    ~_FunctionModelItem();

    ArgumentList arguments() const;

    void addArgument(ArgumentModelItem item);
    void removeArgument(ArgumentModelItem item);

    CodeModel::FunctionType functionType() const;
    void setFunctionType(CodeModel::FunctionType functionType);

    bool isVirtual() const;
    void setVirtual(bool isVirtual);

    bool isInline() const;
    void setInline(bool isInline);

    bool isExplicit() const;
    void setExplicit(bool isExplicit);

    bool isInvokable() const; // Qt
    void setInvokable(bool isInvokable); // Qt

    bool isAbstract() const;
    void setAbstract(bool isAbstract);

    bool isVariadics() const;
    void setVariadics(bool isVariadics);

    bool isSimilar(FunctionModelItem other) const;

private:
    ArgumentList _M_arguments;
    CodeModel::FunctionType _M_functionType;
    union {
        struct {
            uint _M_isVirtual: 1;
            uint _M_isInline: 1;
            uint _M_isAbstract: 1;
            uint _M_isExplicit: 1;
            uint _M_isVariadics: 1;
            uint _M_isInvokable : 1; // Qt
        };
        uint _M_flags;
    };
};

class _FunctionDefinitionModelItem: public _FunctionModelItem
{
public:
    DECLARE_MODEL_NODE(FunctionDefinition)

    explicit _FunctionDefinitionModelItem(CodeModel *model, int kind = __node_kind)
        : _FunctionModelItem(model, kind) {}
    explicit _FunctionDefinitionModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _FunctionModelItem(model, name, kind) {}
    ~_FunctionDefinitionModelItem();
};

class _VariableModelItem: public _MemberModelItem
{
public:
    DECLARE_MODEL_NODE(Variable)

    explicit _VariableModelItem(CodeModel *model, int kind = __node_kind)
        : _MemberModelItem(model, kind) {}
    explicit _VariableModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _MemberModelItem(model, name, kind) {}
};

class _TypeAliasModelItem: public _CodeModelItem
{
public:
    DECLARE_MODEL_NODE(TypeAlias)

    explicit _TypeAliasModelItem(CodeModel *model, int kind = __node_kind)
        : _CodeModelItem(model, kind) {}
    explicit _TypeAliasModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _CodeModelItem(model, name, kind) {}

    TypeInfo type() const;
    void setType(const TypeInfo &type);

private:
    TypeInfo _M_type;
};

class _EnumModelItem: public _CodeModelItem
{
public:
    DECLARE_MODEL_NODE(Enum)

    explicit _EnumModelItem(CodeModel *model, int kind = __node_kind)
        : _CodeModelItem(model, kind), _M_accessPolicy(CodeModel::Public), _M_anonymous(false)  {}
    explicit _EnumModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _CodeModelItem(model, name, kind), _M_accessPolicy(CodeModel::Public), _M_anonymous(false) {}
    ~_EnumModelItem();

    CodeModel::AccessPolicy accessPolicy() const;
    void setAccessPolicy(CodeModel::AccessPolicy accessPolicy);

    EnumeratorList enumerators() const;
    void addEnumerator(EnumeratorModelItem item);
    void removeEnumerator(EnumeratorModelItem item);
    bool isAnonymous() const;
    void setAnonymous(bool anonymous);

private:
    CodeModel::AccessPolicy _M_accessPolicy;
    EnumeratorList _M_enumerators;
    bool _M_anonymous;
};

class _EnumeratorModelItem: public _CodeModelItem
{
public:
    DECLARE_MODEL_NODE(Enumerator)

    explicit _EnumeratorModelItem(CodeModel *model, int kind = __node_kind)
        : _CodeModelItem(model, kind) {}
    explicit _EnumeratorModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _CodeModelItem(model, name, kind) {}
    ~_EnumeratorModelItem();

    QString value() const;
    void setValue(const QString &value);

private:
    QString _M_value;
};

class _TemplateParameterModelItem: public _CodeModelItem
{
public:
    DECLARE_MODEL_NODE(TemplateParameter)

    explicit _TemplateParameterModelItem(CodeModel *model, int kind = __node_kind)
        : _CodeModelItem(model, kind), _M_defaultValue(false) {}
    explicit _TemplateParameterModelItem(CodeModel *model, const QString &name, int kind = __node_kind)
        : _CodeModelItem(model, name, kind), _M_defaultValue(false) {}
    ~_TemplateParameterModelItem();

    TypeInfo type() const;
    void setType(const TypeInfo &type);

    bool defaultValue() const;
    void setDefaultValue(bool defaultValue);

private:
    TypeInfo _M_type;
    bool _M_defaultValue;
};

#endif // CODEMODEL_H

// kate: space-indent on; indent-width 2; replace-tabs on;
