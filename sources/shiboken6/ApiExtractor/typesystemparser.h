/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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
#ifndef TYPESYSTEMPARSER_H
#define TYPESYSTEMPARSER_H

#include "typesystem.h"
#include "modifications.h"

#include <QtCore/QStack>
#include <QtCore/QHash>
#include <QtCore/QScopedPointer>

QT_FORWARD_DECLARE_CLASS(QVersionNumber)
QT_FORWARD_DECLARE_CLASS(QXmlStreamAttributes)
QT_FORWARD_DECLARE_CLASS(QXmlStreamReader)

class ConditionalStreamReader;

class TypeSystemEntityResolver;
class TypeDatabase;
class StackElement
{
    public:
        enum ElementType {
            None = 0x0,

            // Type tags (0x1, ... , 0xff)
            ObjectTypeEntry             = 0x1,
            ValueTypeEntry              = 0x2,
            InterfaceTypeEntry          = 0x3,
            NamespaceTypeEntry          = 0x4,
            ComplexTypeEntryMask        = 0x7,

            // Non-complex type tags (0x8, 0x9, ... , 0xf)
            PrimitiveTypeEntry          = 0x8,
            EnumTypeEntry               = 0x9,
            ContainerTypeEntry          = 0xa,
            FunctionTypeEntry           = 0xb,
            CustomTypeEntry             = 0xc,
            SmartPointerTypeEntry       = 0xd,
            TypedefTypeEntry            = 0xe,
            TypeEntryMask               = 0xf,

            // Documentation tags
            InjectDocumentation         = 0x10,
            ModifyDocumentation         = 0x20,
            DocumentationMask           = 0xf0,

            // Simple tags (0x100, 0x200, ... , 0xf00)
            ExtraIncludes               = 0x0100,
            Include                     = 0x0200,
            ModifyFunction              = 0x0300,
            ModifyField                 = 0x0400,
            Root                        = 0x0500,
            CustomMetaConstructor       = 0x0600,
            CustomMetaDestructor        = 0x0700,
            SuppressedWarning           = 0x0900,
            Rejection                   = 0x0a00,
            LoadTypesystem              = 0x0b00,
            RejectEnumValue             = 0x0c00,
            Template                    = 0x0d00,
            TemplateInstanceEnum        = 0x0e00,
            Replace                     = 0x0f00,
            AddFunction                 = 0x1000,
            DeclareFunction             = 0x1100,
            NativeToTarget              = 0x1200,
            TargetToNative              = 0x1300,
            AddConversion               = 0x1400,
            SystemInclude               = 0x1500,
            Property                    = 0x1600,
            SimpleMask                  = 0x3f00,

            // Code snip tags (0x1000, 0x2000, ... , 0xf000)
            InjectCode                  = 0x4000,
            InjectCodeInFunction        = 0x8000,
            CodeSnipMask                = 0xc000,

            // Function modifier tags (0x010000, 0x020000, ... , 0xf00000)
            Rename                      = 0x040000, // (modify-argument)
            ModifyArgument              = 0x080000,
            Thread                      = 0x100000,
            FunctionModifiers           = 0xff0000,

            // Argument modifier tags (0x01000000 ... 0xf0000000)
            ConversionRule              = 0x01000000,
            ReplaceType                 = 0x02000000,
            ReplaceDefaultExpression    = 0x04000000,
            RemoveArgument              = 0x08000000,
            DefineOwnership             = 0x10000000,
            RemoveDefaultExpression     = 0x20000000,
            NoNullPointers              = 0x40000000,
            ReferenceCount              = 0x80000000,
            ParentOwner                 = 0x90000000,
            Array                       = 0xA0000000,
            ArgumentModifiers           = 0xff000000
        };

        StackElement(StackElement *p) : entry(nullptr), type(None), parent(p) { }

        TypeEntry* entry;
        ElementType type;
        StackElement *parent;

        union {
            TemplateInstance* templateInstance;
            TemplateEntry* templateEntry;
            CustomFunction* customFunction;
        } value;
};

struct StackElementContext
{
    CodeSnipList codeSnips;
    AddedFunctionList addedFunctions;
    FunctionModificationList functionMods;
    FieldModificationList fieldMods;
    DocModificationList docModifications;
    int addedFunctionModificationIndex = -1;
};

class TypeSystemParser
{
public:
    Q_DISABLE_COPY(TypeSystemParser)

    TypeSystemParser(TypeDatabase* database, bool generate);
    ~TypeSystemParser();

    bool parse(ConditionalStreamReader &reader);

    QString errorString() const { return m_error; }

private:
    bool parseXml(ConditionalStreamReader &reader);
    bool setupSmartPointerInstantiations();
    bool startElement(const ConditionalStreamReader &reader);
    SmartPointerTypeEntry *parseSmartPointerEntry(const ConditionalStreamReader &,
                                                  const QString &name,
                                                  const QVersionNumber &since,
                                                  QXmlStreamAttributes *attributes);
    bool endElement(QStringView localName);
    template <class String> // QString/QStringRef
    bool characters(const String &ch);

    bool importFileElement(const QXmlStreamAttributes &atts);

    const TypeEntry *currentParentTypeEntry() const;
    bool checkRootElement();
    bool applyCommonAttributes(const ConditionalStreamReader &reader, TypeEntry *type,
                               QXmlStreamAttributes *attributes);
    PrimitiveTypeEntry *
        parsePrimitiveTypeEntry(const ConditionalStreamReader &, const QString &name,
                                const QVersionNumber &since, QXmlStreamAttributes *);
    CustomTypeEntry *
        parseCustomTypeEntry(const ConditionalStreamReader &, const QString &name,
                             const QVersionNumber &since, QXmlStreamAttributes *);
    ContainerTypeEntry *
        parseContainerTypeEntry(const ConditionalStreamReader &, const QString &name,
                                 const QVersionNumber &since, QXmlStreamAttributes *);
    EnumTypeEntry *
        parseEnumTypeEntry(const ConditionalStreamReader &, const QString &name,
                           const QVersionNumber &since, QXmlStreamAttributes *);
    FlagsTypeEntry *
        parseFlagsEntry(const ConditionalStreamReader &, EnumTypeEntry *enumEntry,
                        QString flagName, const QVersionNumber &since,
                        QXmlStreamAttributes *);

    NamespaceTypeEntry *
        parseNamespaceTypeEntry(const ConditionalStreamReader &,
                                const QString &name, const QVersionNumber &since,
                                QXmlStreamAttributes *attributes);

    ValueTypeEntry *
        parseValueTypeEntry(const ConditionalStreamReader &, const QString &name,
                            const QVersionNumber &since, QXmlStreamAttributes *);
    FunctionTypeEntry *
        parseFunctionTypeEntry(const ConditionalStreamReader &, const QString &name,
                               const QVersionNumber &since, QXmlStreamAttributes *);
    TypedefEntry *
        parseTypedefEntry(const ConditionalStreamReader &, const QString &name,
                          const QVersionNumber &since, QXmlStreamAttributes *);
    void applyComplexTypeAttributes(const ConditionalStreamReader &, ComplexTypeEntry *ctype,
                                    QXmlStreamAttributes *) const;
    bool parseRenameFunction(const ConditionalStreamReader &, QString *name,
                             QXmlStreamAttributes *);
    bool parseInjectDocumentation(const ConditionalStreamReader &, QXmlStreamAttributes *);
    bool parseModifyDocumentation(const ConditionalStreamReader &, QXmlStreamAttributes *);
    TypeSystemTypeEntry *
        parseRootElement(const ConditionalStreamReader &, const QVersionNumber &since,
                         QXmlStreamAttributes *);
    bool loadTypesystem(const ConditionalStreamReader &, QXmlStreamAttributes *);
    bool parseRejectEnumValue(const ConditionalStreamReader &, QXmlStreamAttributes *);
    bool parseReplaceArgumentType(const ConditionalStreamReader &, const StackElement &topElement,
                                  QXmlStreamAttributes *);
    bool parseCustomConversion(const ConditionalStreamReader &, const StackElement &topElement,
                               QXmlStreamAttributes *);
    bool parseAddConversion(const ConditionalStreamReader &, const StackElement &topElement,
                            QXmlStreamAttributes *);
    bool parseNativeToTarget(const ConditionalStreamReader &, const StackElement &topElement,
                             QXmlStreamAttributes *attributes);
    bool parseModifyArgument(const ConditionalStreamReader &, const StackElement &topElement,
                             QXmlStreamAttributes *attributes);
    bool parseNoNullPointer(const ConditionalStreamReader &, const StackElement &topElement,
                            QXmlStreamAttributes *attributes);
    bool parseDefineOwnership(const ConditionalStreamReader &, const StackElement &topElement,
                              QXmlStreamAttributes *);
    bool parseRename(const ConditionalStreamReader &, const StackElement &topElement,
                     QXmlStreamAttributes *);
    bool parseModifyField(const ConditionalStreamReader &, QXmlStreamAttributes *);
    bool parseAddFunction(const ConditionalStreamReader &, const StackElement &topElement,
                          StackElement::ElementType t, QXmlStreamAttributes *);
    bool parseProperty(const ConditionalStreamReader &, const StackElement &topElement,
                       QXmlStreamAttributes *);
    bool parseModifyFunction(const ConditionalStreamReader &, const StackElement &topElement,
                             QXmlStreamAttributes *);
    bool parseReplaceDefaultExpression(const ConditionalStreamReader &,
                                       const StackElement &topElement, QXmlStreamAttributes *);
    static CustomFunction *
        parseCustomMetaConstructor(const ConditionalStreamReader &,
                                   StackElement::ElementType type,
                                   const StackElement &topElement, QXmlStreamAttributes *);
     bool parseReferenceCount(const ConditionalStreamReader &, const StackElement &topElement,
                              QXmlStreamAttributes *);
     bool parseParentOwner(const ConditionalStreamReader &, const StackElement &topElement,
                           QXmlStreamAttributes *);
     bool readFileSnippet(QXmlStreamAttributes *attributes, CodeSnip *snip);
     bool parseInjectCode(const ConditionalStreamReader &, const StackElement &topElement,
                          StackElement* element, QXmlStreamAttributes *);
     bool parseInclude(const ConditionalStreamReader &, const StackElement &topElement,
                       TypeEntry *entry, QXmlStreamAttributes *);
     bool parseSystemInclude(const ConditionalStreamReader &, QXmlStreamAttributes *);
     TemplateInstance
         *parseTemplateInstanceEnum(const ConditionalStreamReader &, const StackElement &topElement,
                                    QXmlStreamAttributes *);
     bool parseReplace(const ConditionalStreamReader &, const StackElement &topElement,
                       StackElement *element, QXmlStreamAttributes *);

    TypeDatabase* m_database;
    StackElement* m_current = nullptr;
    StackElement* m_currentDroppedEntry = nullptr;
    int m_currentDroppedEntryDepth = 0;
    int m_ignoreDepth = 0;
    QString m_defaultPackage;
    QString m_defaultSuperclass;
    TypeSystem::ExceptionHandling m_exceptionHandling = TypeSystem::ExceptionHandling::Unspecified;
    TypeSystem::AllowThread m_allowThread = TypeSystem::AllowThread::Unspecified;
    QString m_error;
    const TypeEntry::CodeGeneration m_generate;

    EnumTypeEntry* m_currentEnum = nullptr;
    QStack<StackElementContext*> m_contextStack;

    QString m_currentSignature;
    QString m_currentPath;
    QString m_currentFile;
    QScopedPointer<TypeSystemEntityResolver> m_entityResolver;
    QHash<SmartPointerTypeEntry *, QString> m_smartPointerInstantiations;
};

#endif // TYPESYSTEMPARSER_H
