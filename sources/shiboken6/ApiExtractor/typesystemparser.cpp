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

#include "typesystemparser.h"
#include "typedatabase.h"
#include "messages.h"
#include "reporthandler.h"
#include "sourcelocation.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QStringView>
#include <QtCore/QStringAlgorithms>
#include <QtCore/QVersionNumber>
#include <QtCore/QXmlStreamAttributes>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamEntityResolver>

#include <algorithm>
#include <optional>
#include <memory>

static inline QString allowThreadAttribute() { return QStringLiteral("allow-thread"); }
static inline QString colonColon() { return QStringLiteral("::"); }
static inline QString copyableAttribute() { return QStringLiteral("copyable"); }
static inline QString accessAttribute() { return QStringLiteral("access"); }
static inline QString actionAttribute() { return QStringLiteral("action"); }
static inline QString quoteAfterLineAttribute() { return QStringLiteral("quote-after-line"); }
static inline QString quoteBeforeLineAttribute() { return QStringLiteral("quote-before-line"); }
static inline QString textAttribute() { return QStringLiteral("text"); }
static inline QString nameAttribute() { return QStringLiteral("name"); }
static inline QString sinceAttribute() { return QStringLiteral("since"); }
static inline QString untilAttribute() { return QStringLiteral("until"); }
static inline QString defaultSuperclassAttribute() { return QStringLiteral("default-superclass"); }
static inline QString deleteInMainThreadAttribute() { return QStringLiteral("delete-in-main-thread"); }
static inline QString deprecatedAttribute() { return QStringLiteral("deprecated"); }
static inline QString disableWrapperAttribute() { return QStringLiteral("disable-wrapper"); }
static inline QString exceptionHandlingAttribute() { return QStringLiteral("exception-handling"); }
static inline QString extensibleAttribute() { return QStringLiteral("extensible"); }
static inline QString fileNameAttribute() { return QStringLiteral("file-name"); }
static inline QString flagsAttribute() { return QStringLiteral("flags"); }
static inline QString forceAbstractAttribute() { return QStringLiteral("force-abstract"); }
static inline QString forceIntegerAttribute() { return QStringLiteral("force-integer"); }
static inline QString formatAttribute() { return QStringLiteral("format"); }
static inline QString generateUsingAttribute() { return QStringLiteral("generate-using"); }
static inline QString classAttribute() { return QStringLiteral("class"); }
static inline QString generateAttribute() { return QStringLiteral("generate"); }
static inline QString generateGetSetDefAttribute() { return QStringLiteral("generate-getsetdef"); }
static inline QString genericClassAttribute() { return QStringLiteral("generic-class"); }
static inline QString indexAttribute() { return QStringLiteral("index"); }
static inline QString invalidateAfterUseAttribute() { return QStringLiteral("invalidate-after-use"); }
static inline QString locationAttribute() { return QStringLiteral("location"); }
static inline QString modifiedTypeAttribute() { return QStringLiteral("modified-type"); }
static inline QString overloadNumberAttribute() { return QStringLiteral("overload-number"); }
static inline QString ownershipAttribute() { return QStringLiteral("owner"); }
static inline QString packageAttribute() { return QStringLiteral("package"); }
static inline QString positionAttribute() { return QStringLiteral("position"); }
static inline QString preferredConversionAttribute() { return QStringLiteral("preferred-conversion"); }
static inline QString preferredTargetLangTypeAttribute() { return QStringLiteral("preferred-target-lang-type"); }
static inline QString removeAttribute() { return QStringLiteral("remove"); }
static inline QString renameAttribute() { return QStringLiteral("rename"); }
static inline QString readAttribute() { return QStringLiteral("read"); }
static inline QString targetLangNameAttribute() { return QStringLiteral("target-lang-name"); }
static inline QString writeAttribute() { return QStringLiteral("write"); }
static inline QString replaceAttribute() { return QStringLiteral("replace"); }
static inline QString toAttribute() { return QStringLiteral("to"); }
static inline QString signatureAttribute() { return QStringLiteral("signature"); }
static inline QString snippetAttribute() { return QStringLiteral("snippet"); }
static inline QString snakeCaseAttribute() { return QStringLiteral("snake-case"); }
static inline QString staticAttribute() { return QStringLiteral("static"); }
static inline QString threadAttribute() { return QStringLiteral("thread"); }
static inline QString sourceAttribute() { return QStringLiteral("source"); }
static inline QString streamAttribute() { return QStringLiteral("stream"); }
static inline QString xPathAttribute() { return QStringLiteral("xpath"); }
static inline QString virtualSlotAttribute() { return QStringLiteral("virtual-slot"); }
static inline QString visibleAttribute() { return QStringLiteral("visible"); }
static inline QString enumIdentifiedByValueAttribute() { return QStringLiteral("identified-by-value"); }

static inline QString noAttributeValue() { return QStringLiteral("no"); }
static inline QString yesAttributeValue() { return QStringLiteral("yes"); }
static inline QString trueAttributeValue() { return QStringLiteral("true"); }
static inline QString falseAttributeValue() { return QStringLiteral("false"); }

static QList<CustomConversion *> customConversionsForReview;

// Set a regular expression for rejection from text. By legacy, those are fixed
// strings, except for '*' meaning 'match all'. Enclosing in "^..$"
// indicates regular expression.
static bool setRejectionRegularExpression(const QString &patternIn,
                                          QRegularExpression *re,
                                          QString *errorMessage)
{
    QString pattern;
    if (patternIn.startsWith(QLatin1Char('^')) && patternIn.endsWith(QLatin1Char('$')))
        pattern = patternIn;
    else if (patternIn == QLatin1String("*"))
        pattern = QStringLiteral("^.*$");
    else
        pattern = QLatin1Char('^') + QRegularExpression::escape(patternIn) + QLatin1Char('$');
    re->setPattern(pattern);
    if (!re->isValid()) {
        *errorMessage = msgInvalidRegularExpression(patternIn, re->errorString());
        return false;
    }
    return true;
}

// Extract a snippet from a file within annotation "// @snippet label".
std::optional<QString>
    extractSnippet(const QString &code, const QString &snippetLabel)
{
    if (snippetLabel.isEmpty())
        return code;
    const QString pattern = QStringLiteral(R"(^\s*//\s*@snippet\s+)")
        + QRegularExpression::escape(snippetLabel)
        + QStringLiteral(R"(\s*$)");
    const QRegularExpression snippetRe(pattern);
    Q_ASSERT(snippetRe.isValid());

    bool useLine = false;
    bool foundLabel = false;
    QString result;
    const auto lines = QStringView{code}.split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        if (snippetRe.match(line).hasMatch()) {
            foundLabel = true;
            useLine = !useLine;
            if (!useLine)
                break; // End of snippet reached
        } else if (useLine)
            result += line.toString() + QLatin1Char('\n');
    }
    if (!foundLabel)
        return {};
    return CodeSnipAbstract::fixSpaces(result);
}

template <class EnumType, Qt::CaseSensitivity cs = Qt::CaseInsensitive>
struct EnumLookup
{
    QStringView name;
    EnumType value;
};

template <class EnumType, Qt::CaseSensitivity cs>
bool operator==(const EnumLookup<EnumType, cs> &e1, const EnumLookup<EnumType, cs> &e2)
{
    return e1.name.compare(e2.name, cs) == 0;
}

template <class EnumType, Qt::CaseSensitivity cs>
bool operator<(const EnumLookup<EnumType, cs> &e1, const EnumLookup<EnumType, cs> &e2)
{
    return e1.name.compare(e2.name, cs) < 0;
}

// Helper macros to define lookup functions that take a QStringView needle
// and an optional default return value.
#define ENUM_LOOKUP_BEGIN(EnumType, caseSensitivity, functionName) \
static std::optional<EnumType> functionName(QStringView needle) \
{ \
    using HaystackEntry = EnumLookup<EnumType, caseSensitivity>; \
    static const HaystackEntry haystack[] =

#define ENUM_LOOKUP_LINEAR_SEARCH() \
    const auto end = haystack + sizeof(haystack) / sizeof(haystack[0]); \
    const auto it = std::find(haystack, end, HaystackEntry{needle, {} }); \
    if (it != end) \
        return it->value; \
    return {}; \
}

#define ENUM_LOOKUP_BINARY_SEARCH() \
    const auto end = haystack + sizeof(haystack) / sizeof(haystack[0]); \
    const HaystackEntry needleEntry{needle, {} }; \
    const auto lb = std::lower_bound(haystack, end, needleEntry); \
    if (lb != end && *lb == needleEntry) \
        return lb->value; \
    return {}; \
}

ENUM_LOOKUP_BEGIN(TypeSystem::AllowThread, Qt::CaseInsensitive,
                  allowThreadFromAttribute)
    {
        {u"yes", TypeSystem::AllowThread::Allow},
        {u"true", TypeSystem::AllowThread::Allow},
        {u"auto", TypeSystem::AllowThread::Auto},
        {u"no", TypeSystem::AllowThread::Disallow},
        {u"false", TypeSystem::AllowThread::Disallow},
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(TypeSystem::Language, Qt::CaseInsensitive,
                  languageFromAttribute)
    {
        {u"all", TypeSystem::All}, // sorted!
        {u"native", TypeSystem::NativeCode}, // em algum lugar do cpp
        {u"shell", TypeSystem::ShellCode}, // coloca no header, mas antes da declaracao da classe
        {u"target", TypeSystem::TargetLangCode}  // em algum lugar do cpp
    };
ENUM_LOOKUP_BINARY_SEARCH()

ENUM_LOOKUP_BEGIN(TypeSystem::Ownership, Qt::CaseInsensitive,
                   ownershipFromFromAttribute)
    {
        {u"target", TypeSystem::TargetLangOwnership},
        {u"c++", TypeSystem::CppOwnership},
        {u"default", TypeSystem::DefaultOwnership}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(AddedFunction::Access, Qt::CaseInsensitive,
                  addedFunctionAccessFromAttribute)
    {
        {u"public", AddedFunction::Public},
        {u"protected", AddedFunction::Protected},
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(FunctionModification::ModifierFlag, Qt::CaseSensitive,
                  modifierFromAttribute)
    {
        {u"private", FunctionModification::Private},
        {u"public", FunctionModification::Public},
        {u"protected", FunctionModification::Protected},
        {u"friendly", FunctionModification::Friendly},
        {u"rename", FunctionModification::Rename},
        {u"final", FunctionModification::Final},
        {u"non-final", FunctionModification::NonFinal}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(ReferenceCount::Action, Qt::CaseInsensitive,
                  referenceCountFromAttribute)
    {
        {u"add", ReferenceCount::Add},
        {u"add-all", ReferenceCount::AddAll},
        {u"remove", ReferenceCount::Remove},
        {u"set", ReferenceCount::Set},
        {u"ignore", ReferenceCount::Ignore}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(ArgumentOwner::Action, Qt::CaseInsensitive,
                  argumentOwnerActionFromAttribute)
    {
        {u"add", ArgumentOwner::Add},
        {u"remove", ArgumentOwner::Remove}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(TypeSystem::CodeSnipPosition, Qt::CaseInsensitive,
                  codeSnipPositionFromAttribute)
    {
        {u"beginning", TypeSystem::CodeSnipPositionBeginning},
        {u"end", TypeSystem::CodeSnipPositionEnd},
        {u"declaration", TypeSystem::CodeSnipPositionDeclaration}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(Include::IncludeType, Qt::CaseInsensitive,
                  locationFromAttribute)
    {
        {u"global", Include::IncludePath},
        {u"local", Include::LocalPath},
        {u"target", Include::TargetLangImport}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(TypeSystem::DocModificationMode, Qt::CaseInsensitive,
                  docModificationFromAttribute)
    {
        {u"append", TypeSystem::DocModificationAppend},
        {u"prepend", TypeSystem::DocModificationPrepend},
        {u"replace", TypeSystem::DocModificationReplace}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(ContainerTypeEntry::ContainerKind, Qt::CaseSensitive,
                  containerTypeFromAttribute)
    {
        {u"list", ContainerTypeEntry::ListContainer},
        {u"string-list", ContainerTypeEntry::StringListContainer},
        {u"linked-list", ContainerTypeEntry::LinkedListContainer},
        {u"vector", ContainerTypeEntry::VectorContainer},
        {u"stack", ContainerTypeEntry::StackContainer},
        {u"queue", ContainerTypeEntry::QueueContainer},
        {u"set", ContainerTypeEntry::SetContainer},
        {u"map", ContainerTypeEntry::MapContainer},
        {u"multi-map", ContainerTypeEntry::MultiMapContainer},
        {u"hash", ContainerTypeEntry::HashContainer},
        {u"multi-hash", ContainerTypeEntry::MultiHashContainer},
        {u"pair", ContainerTypeEntry::PairContainer}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(TypeRejection::MatchType, Qt::CaseSensitive,
                  typeRejectionFromAttribute)
    {
        {u"class", TypeRejection::ExcludeClass},
        {u"function-name", TypeRejection::Function},
        {u"field-name", TypeRejection::Field},
        {u"enum-name", TypeRejection::Enum },
        {u"argument-type", TypeRejection::ArgumentType},
        {u"return-type", TypeRejection::ReturnType}
    };
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(TypeSystem::ExceptionHandling, Qt::CaseSensitive,
                  exceptionHandlingFromAttribute)
{
    {u"no", TypeSystem::ExceptionHandling::Off},
    {u"false", TypeSystem::ExceptionHandling::Off},
    {u"auto-off", TypeSystem::ExceptionHandling::AutoDefaultToOff},
    {u"auto-on", TypeSystem::ExceptionHandling::AutoDefaultToOn},
    {u"yes", TypeSystem::ExceptionHandling::On},
    {u"true", TypeSystem::ExceptionHandling::On},
};
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(StackElement::ElementType, Qt::CaseInsensitive,
                  elementFromTag)
    {
        {u"add-conversion", StackElement::AddConversion}, // sorted!
        {u"add-function", StackElement::AddFunction},
        {u"array", StackElement::Array},
        {u"container-type", StackElement::ContainerTypeEntry},
        {u"conversion-rule", StackElement::ConversionRule},
        {u"custom-constructor", StackElement::CustomMetaConstructor},
        {u"custom-destructor", StackElement::CustomMetaDestructor},
        {u"custom-type", StackElement::CustomTypeEntry},
        {u"declare-function", StackElement::DeclareFunction},
        {u"define-ownership", StackElement::DefineOwnership},
        {u"enum-type", StackElement::EnumTypeEntry},
        {u"extra-includes", StackElement::ExtraIncludes},
        {u"function", StackElement::FunctionTypeEntry},
        {u"include", StackElement::Include},
        {u"inject-code", StackElement::InjectCode},
        {u"inject-documentation", StackElement::InjectDocumentation},
        {u"insert-template", StackElement::TemplateInstanceEnum},
        {u"interface-type", StackElement::InterfaceTypeEntry},
        {u"load-typesystem", StackElement::LoadTypesystem},
        {u"modify-argument", StackElement::ModifyArgument},
        {u"modify-documentation", StackElement::ModifyDocumentation},
        {u"modify-field", StackElement::ModifyField},
        {u"modify-function", StackElement::ModifyFunction},
        {u"namespace-type", StackElement::NamespaceTypeEntry},
        {u"native-to-target", StackElement::NativeToTarget},
        {u"no-null-pointer", StackElement::NoNullPointers},
        {u"object-type", StackElement::ObjectTypeEntry},
        {u"parent", StackElement::ParentOwner},
        {u"primitive-type", StackElement::PrimitiveTypeEntry},
        {u"property", StackElement::Property},
        {u"reference-count", StackElement::ReferenceCount},
        {u"reject-enum-value", StackElement::RejectEnumValue},
        {u"rejection", StackElement::Rejection},
        {u"remove-argument", StackElement::RemoveArgument},
        {u"remove-default-expression", StackElement::RemoveDefaultExpression},
        {u"rename", StackElement::Rename}, // ### fixme PySide7: remove
        {u"replace", StackElement::Replace},
        {u"replace-default-expression", StackElement::ReplaceDefaultExpression},
        {u"replace-type", StackElement::ReplaceType},
        {u"smart-pointer-type", StackElement::SmartPointerTypeEntry},
        {u"suppress-warning", StackElement::SuppressedWarning},
        {u"system-include", StackElement::SystemInclude},
        {u"target-to-native", StackElement::TargetToNative},
        {u"template", StackElement::Template},
        {u"typedef-type", StackElement::TypedefTypeEntry},
        {u"typesystem", StackElement::Root},
        {u"value-type", StackElement::ValueTypeEntry},
    };
ENUM_LOOKUP_BINARY_SEARCH()


ENUM_LOOKUP_BEGIN(TypeSystem::SnakeCase, Qt::CaseSensitive,
                  snakeCaseFromAttribute)
{
    {u"no", TypeSystem::SnakeCase::Disabled},
    {u"false", TypeSystem::SnakeCase::Disabled},
    {u"yes", TypeSystem::SnakeCase::Enabled},
    {u"true", TypeSystem::SnakeCase::Enabled},
    {u"both", TypeSystem::SnakeCase::Both},
};
ENUM_LOOKUP_LINEAR_SEARCH()

ENUM_LOOKUP_BEGIN(TypeSystem::Visibility, Qt::CaseSensitive,
                  visibilityFromAttribute)
{
    {u"no", TypeSystem::Visibility::Invisible},
    {u"false", TypeSystem::Visibility::Invisible},
    {u"auto", TypeSystem::Visibility::Auto},
    {u"yes", TypeSystem::Visibility::Visible},
    {u"true", TypeSystem::Visibility::Visible},
};
ENUM_LOOKUP_LINEAR_SEARCH()

static int indexOfAttribute(const QXmlStreamAttributes &atts,
                            QStringView name)
{
    for (int i = 0, size = atts.size(); i < size; ++i) {
        if (atts.at(i).qualifiedName() == name)
            return i;
    }
    return -1;
}

static QString msgMissingAttribute(const QString &a)
{
    return QLatin1String("Required attribute '") + a
        + QLatin1String("' missing.");
}

QTextStream &operator<<(QTextStream &str, const QXmlStreamAttribute &attribute)
{
    str << attribute.qualifiedName() << "=\"" << attribute.value() << '"';
    return str;
}

static QString msgInvalidAttributeValue(const QXmlStreamAttribute &attribute)
{
    QString result;
    QTextStream(&result) << "Invalid attribute value:" << attribute;
    return result;
}

static QString msgUnusedAttributes(QStringView tag, const QXmlStreamAttributes &attributes)
{
    QString result;
    QTextStream str(&result);
    str << attributes.size() << " attributes(s) unused on <" << tag << ">: ";
    for (int i = 0, size = attributes.size(); i < size; ++i) {
        if (i)
            str << ", ";
        str << attributes.at(i);
    }
    return result;
}

// QXmlStreamEntityResolver::resolveEntity(publicId, systemId) is not
// implemented; resolve via undeclared entities instead.
class TypeSystemEntityResolver : public QXmlStreamEntityResolver
{
public:
    explicit TypeSystemEntityResolver(const QString &currentPath) :
        m_currentPath(currentPath) {}

    QString resolveUndeclaredEntity(const QString &name) override;

private:
    QString readFile(const QString &entityName, QString *errorMessage) const;

    const QString m_currentPath;
    QHash<QString, QString> m_cache;
};

QString TypeSystemEntityResolver::readFile(const QString &entityName, QString *errorMessage) const
{
    QString fileName = entityName;
    if (!fileName.contains(QLatin1Char('.')))
        fileName += QLatin1String(".xml");
    QString path = TypeDatabase::instance()->modifiedTypesystemFilepath(fileName, m_currentPath);
    if (!QFileInfo::exists(path)) // PySide6-specific hack
        fileName.prepend(QLatin1String("typesystem_"));
    path = TypeDatabase::instance()->modifiedTypesystemFilepath(fileName, m_currentPath);
    if (!QFileInfo::exists(path)) {
        *errorMessage = QLatin1String("Unable to resolve: ") + entityName;
        return QString();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *errorMessage = msgCannotOpenForReading(file);
        return QString();
    }
    QString result = QString::fromUtf8(file.readAll()).trimmed();
    // Remove license header comments on which QXmlStreamReader chokes
    if (result.startsWith(QLatin1String("<!--"))) {
        const int commentEnd = result.indexOf(QLatin1String("-->"));
        if (commentEnd != -1) {
            result.remove(0, commentEnd + 3);
            result = result.trimmed();
        }
    }
    return result;
}

QString TypeSystemEntityResolver::resolveUndeclaredEntity(const QString &name)
{
    auto it = m_cache.find(name);
    if (it == m_cache.end()) {
        QString errorMessage;
        it = m_cache.insert(name, readFile(name, &errorMessage));
        if (it.value().isEmpty()) { // The parser will fail and display the line number.
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgCannotResolveEntity(name, errorMessage)));
        }
    }
    return it.value();
}

TypeSystemParser::TypeSystemParser(TypeDatabase *database, bool generate) :
    m_database(database),
    m_generate(generate ? TypeEntry::GenerateCode : TypeEntry::GenerateForSubclass)
{
}

TypeSystemParser::~TypeSystemParser() = default;

static QString readerFileName(const QXmlStreamReader &reader)
{
    const auto *file = qobject_cast<const QFile *>(reader.device());
    return file != nullptr ? file->fileName() : QString();
}

static QString msgReaderMessage(const QXmlStreamReader &reader,
                                const char *type,
                                const QString &what)
{
    QString message;
    QTextStream str(&message);
    const QString fileName = readerFileName(reader);
    if (fileName.isEmpty())
        str << "<stdin>:";
    else
        str << QDir::toNativeSeparators(fileName) << ':';
    // Use a tab separator like SourceLocation for suppression detection
    str << reader.lineNumber() << ':' << reader.columnNumber()
        << ":\t" << type << ": " << what;
    return message;
}

static QString msgReaderWarning(const QXmlStreamReader &reader, const QString &what)
{
    return  msgReaderMessage(reader, "Warning", what);
}

static QString msgReaderError(const QXmlStreamReader &reader, const QString &what)
{
    return  msgReaderMessage(reader, "Error", what);
}

static QString msgUnimplementedElementWarning(const QXmlStreamReader &reader,
                                              QStringView name)
{
    QString message;
    QTextStream(&message) << "The element \"" << name
        << "\" is not implemented.";
    return msgReaderMessage(reader, "Warning", message);
}

static QString msgUnimplementedAttributeWarning(const QXmlStreamReader &reader,
                                                QStringView name)
{
    QString message;
    QTextStream(&message) <<  "The attribute \"" << name
        << "\" is not implemented.";
    return msgReaderMessage(reader, "Warning", message);
}

static inline QString msgUnimplementedAttributeWarning(const QXmlStreamReader &reader,
                                                       const QXmlStreamAttribute &attribute)
{
    return msgUnimplementedAttributeWarning(reader, attribute.qualifiedName());
}

static QString
    msgUnimplementedAttributeValueWarning(const QXmlStreamReader &reader,
                                          QStringView name, QStringView value)
{
    QString message;
    QTextStream(&message) << "The value \"" << value
        << "\" of the attribute \"" << name << "\" is not implemented.";
    return msgReaderMessage(reader, "Warning", message);
}

static inline
    QString msgUnimplementedAttributeValueWarning(const QXmlStreamReader &reader,
                                                  const QXmlStreamAttribute &attribute)
{
    return msgUnimplementedAttributeValueWarning(reader,
                                                 attribute.qualifiedName(),
                                                 attribute.value());
}

static bool addRejection(TypeDatabase *database, QXmlStreamAttributes *attributes,
                         QString *errorMessage)
{
    const int classIndex = indexOfAttribute(*attributes, classAttribute());
    if (classIndex == -1) {
        *errorMessage = msgMissingAttribute(classAttribute());
        return false;
    }

    TypeRejection rejection;
    const QString className = attributes->takeAt(classIndex).value().toString();
    if (!setRejectionRegularExpression(className, &rejection.className, errorMessage))
        return false;

    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto &attribute = attributes->at(i);
        const auto name = attribute.qualifiedName();
        const auto typeOpt = typeRejectionFromAttribute(name);
        if (!typeOpt.has_value()) {
            *errorMessage = msgInvalidAttributeValue(attribute);
            return false;
        }
        switch (typeOpt.value()) {
        case TypeRejection::Function:
        case TypeRejection::Field:
        case TypeRejection::Enum:
        case TypeRejection::ArgumentType:
        case TypeRejection::ReturnType: {
            const QString pattern = attributes->takeAt(i).value().toString();
            if (!setRejectionRegularExpression(pattern, &rejection.pattern, errorMessage))
                return false;
            rejection.matchType = typeOpt.value();
            database->addRejection(rejection);
            return true;
        }
        case TypeRejection::ExcludeClass:
            break;
        }
    }

    // Special case: When all fields except class are empty, completely exclude class
    if (className == QLatin1String("*")) {
        *errorMessage = QLatin1String("bad reject entry, neither 'class', 'function-name'"
                                      " nor 'field' specified");
        return false;
    }
    rejection.matchType = TypeRejection::ExcludeClass;
    database->addRejection(rejection);
    return true;
}

bool TypeSystemParser::parse(QXmlStreamReader &reader)
{
    m_error.clear();
    m_currentPath.clear();
    m_currentFile.clear();
    m_smartPointerInstantiations.clear();
    const bool result = parseXml(reader) && setupSmartPointerInstantiations();
    m_smartPointerInstantiations.clear();
    return result;
}

bool TypeSystemParser::parseXml(QXmlStreamReader &reader)
{
    const QString fileName = readerFileName(reader);
    if (!fileName.isEmpty()) {
        QFileInfo fi(fileName);
        m_currentPath = fi.absolutePath();
        m_currentFile = fi.absoluteFilePath();
    }
    m_entityResolver.reset(new TypeSystemEntityResolver(m_currentPath));
    reader.setEntityResolver(m_entityResolver.data());

    while (!reader.atEnd()) {
        switch (reader.readNext()) {
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
            m_error = msgReaderError(reader, reader.errorString());
            return false;
        case QXmlStreamReader::StartElement:
            if (!startElement(reader)) {
                m_error = msgReaderError(reader, m_error);
                return false;
            }

            break;
        case QXmlStreamReader::EndElement:
            if (!endElement(reader.name())) {
                m_error = msgReaderError(reader, m_error);
                return false;
            }
            break;
        case QXmlStreamReader::Characters:
            if (!characters(reader.text())) {
                m_error = msgReaderError(reader, m_error);
                return false;
            }
            break;
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    }
    return true;
}

// Split a type list potentially with template types
// "A<B,C>,D" -> ("A<B,C>", "D")
static QStringList splitTypeList(const QString &s)
{
    QStringList result;
    int templateDepth = 0;
    int lastPos = 0;
    const int size = s.size();
    for (int i = 0; i < size; ++i) {
        switch (s.at(i).toLatin1()) {
        case '<':
            ++templateDepth;
            break;
        case '>':
            --templateDepth;
            break;
        case ',':
            if (templateDepth == 0) {
                result.append(s.mid(lastPos, i - lastPos).trimmed());
                lastPos = i + 1;
            }
            break;
        }
    }
    if (lastPos < size)
        result.append(s.mid(lastPos, size - lastPos).trimmed());
    return result;
}

bool TypeSystemParser::setupSmartPointerInstantiations()
{
    for (auto it = m_smartPointerInstantiations.cbegin(),
         end = m_smartPointerInstantiations.cend(); it != end; ++it) {
        auto smartPointerEntry = it.key();
        const auto instantiationNames = splitTypeList(it.value());
        SmartPointerTypeEntry::Instantiations instantiations;
        instantiations.reserve(instantiationNames.size());
        for (const auto &instantiationName : instantiationNames) {
            const auto types = m_database->findCppTypes(instantiationName);
            if (types.isEmpty()) {
                m_error =
                    msgCannotFindTypeEntryForSmartPointer(instantiationName,
                                                          smartPointerEntry->name());
                return false;
            }
            if (types.size() > 1) {
                m_error = msgAmbiguousTypesFound(instantiationName, types);
                return false;
            }
            instantiations.append(types.constFirst());
        }
        smartPointerEntry->setInstantiations(instantiations);
    }
    return true;
}

bool TypeSystemParser::endElement(QStringView localName)
{
    if (m_ignoreDepth) {
        --m_ignoreDepth;
        return true;
    }

    if (m_currentDroppedEntry) {
        if (m_currentDroppedEntryDepth == 1) {
            m_current = m_currentDroppedEntry->parent;
            delete m_currentDroppedEntry;
            m_currentDroppedEntry = nullptr;
            m_currentDroppedEntryDepth = 0;
        } else {
            --m_currentDroppedEntryDepth;
        }
        return true;
    }

    if (!localName.compare(QLatin1String("import-file"), Qt::CaseInsensitive))
        return true;

    if (!m_current)
        return true;

    switch (m_current->type) {
    case StackElement::Root:
        if (m_generate == TypeEntry::GenerateCode) {
            TypeDatabase::instance()->addGlobalUserFunctions(m_contextStack.top()->addedFunctions);
            TypeDatabase::instance()->addGlobalUserFunctionModifications(m_contextStack.top()->functionMods);
            for (CustomConversion *customConversion : qAsConst(customConversionsForReview)) {
                const CustomConversion::TargetToNativeConversions &toNatives = customConversion->targetToNativeConversions();
                for (CustomConversion::TargetToNativeConversion *toNative : toNatives)
                    toNative->setSourceType(m_database->findType(toNative->sourceTypeName()));
            }
        }
        break;
    case StackElement::ObjectTypeEntry:
    case StackElement::ValueTypeEntry:
    case StackElement::InterfaceTypeEntry:
    case StackElement::ContainerTypeEntry:
    case StackElement::NamespaceTypeEntry: {
        auto *centry = static_cast<ComplexTypeEntry *>(m_current->entry);
        auto top = m_contextStack.top();
        centry->setAddedFunctions(top->addedFunctions);
        centry->setFunctionModifications(top->functionMods);
        centry->setFieldModifications(top->fieldMods);
        centry->setCodeSnips(top->codeSnips);
        centry->setDocModification(top->docModifications);
    }
    break;

    case StackElement::TypedefTypeEntry: {
        auto *centry = static_cast<TypedefEntry *>(m_current->entry)->target();
        auto top = m_contextStack.top();
        centry->setAddedFunctions(centry->addedFunctions() + top->addedFunctions);
        centry->setFunctionModifications(centry->functionModifications() + top->functionMods);
        centry->setFieldModifications(centry->fieldModifications() + top->fieldMods);
        centry->setCodeSnips(centry->codeSnips() + top->codeSnips);
        centry->setDocModification(centry->docModifications() + top->docModifications);
    }
    break;

    case StackElement::AddFunction: {
        // Leaving add-function: Assign all modifications to the added function
        StackElementContext *top = m_contextStack.top();
        const int modIndex = top->addedFunctionModificationIndex;
        top->addedFunctionModificationIndex = -1;
        Q_ASSERT(modIndex >= 0);
        Q_ASSERT(!top->addedFunctions.isEmpty());
        while (modIndex < top->functionMods.size())
            top->addedFunctions.last()->modifications.append(top->functionMods.takeAt(modIndex));
    }
        break;
    case StackElement::NativeToTarget:
    case StackElement::AddConversion: {
        CustomConversion* customConversion = static_cast<TypeEntry*>(m_current->entry)->customConversion();
        if (!customConversion) {
            m_error = QLatin1String("CustomConversion object is missing.");
            return false;
        }

        QString code = m_contextStack.top()->codeSnips.takeLast().code();
        if (m_current->type == StackElement::AddConversion) {
            if (customConversion->targetToNativeConversions().isEmpty()) {
                m_error = QLatin1String("CustomConversion's target to native conversions missing.");
                return false;
            }
            customConversion->targetToNativeConversions().last()->setConversion(code);
        } else {
            customConversion->setNativeToTargetConversion(code);
        }
    }
    break;
    case StackElement::CustomMetaConstructor: {
        m_current->entry->setCustomConstructor(*m_current->value.customFunction);
        delete m_current->value.customFunction;
    }
    break;
    case StackElement::CustomMetaDestructor: {
        m_current->entry->setCustomDestructor(*m_current->value.customFunction);
        delete m_current->value.customFunction;
    }
    break;
    case StackElement::EnumTypeEntry:
        m_current->entry->setDocModification(m_contextStack.top()->docModifications);
        m_contextStack.top()->docModifications = DocModificationList();
        m_currentEnum = nullptr;
        break;
    case StackElement::Template:
        m_database->addTemplate(m_current->value.templateEntry);
        break;
    case StackElement::TemplateInstanceEnum:
        switch (m_current->parent->type) {
        case StackElement::InjectCode:
            if (m_current->parent->parent->type == StackElement::Root) {
                CodeSnipList snips = m_current->parent->entry->codeSnips();
                CodeSnip snip = snips.takeLast();
                snip.addTemplateInstance(m_current->value.templateInstance);
                snips.append(snip);
                m_current->parent->entry->setCodeSnips(snips);
                break;
            }
            Q_FALLTHROUGH();
        case StackElement::NativeToTarget:
        case StackElement::AddConversion:
            m_contextStack.top()->codeSnips.last().addTemplateInstance(m_current->value.templateInstance);
            break;
        case StackElement::Template:
            m_current->parent->value.templateEntry->addTemplateInstance(m_current->value.templateInstance);
            break;
        case StackElement::CustomMetaConstructor:
        case StackElement::CustomMetaDestructor:
            m_current->parent->value.customFunction->addTemplateInstance(m_current->value.templateInstance);
            break;
        case StackElement::ConversionRule:
            m_contextStack.top()->functionMods.last().argument_mods().last().conversionRules().last().addTemplateInstance(m_current->value.templateInstance);
            break;
        case StackElement::InjectCodeInFunction:
            m_contextStack.top()->functionMods.last().snips().last().addTemplateInstance(m_current->value.templateInstance);
            break;
        default:
            break; // nada
        }
        break;
    default:
        break;
    }

    switch (m_current->type) {
    case StackElement::Root:
    case StackElement::NamespaceTypeEntry:
    case StackElement::InterfaceTypeEntry:
    case StackElement::ObjectTypeEntry:
    case StackElement::ValueTypeEntry:
    case StackElement::PrimitiveTypeEntry:
    case StackElement::TypedefTypeEntry:
    case StackElement::ContainerTypeEntry:
        delete m_contextStack.pop();
        break;
    default:
        break;
    }

    StackElement *child = m_current;
    m_current = m_current->parent;
    delete(child);

    return true;
}

template <class String> // QString/QStringRef
bool TypeSystemParser::characters(const String &ch)
{
    if (m_currentDroppedEntry || m_ignoreDepth)
        return true;

    if (m_current->type == StackElement::Template) {
        m_current->value.templateEntry->addCode(ch);
        return true;
    }

    if (m_current->type == StackElement::CustomMetaConstructor || m_current->type == StackElement::CustomMetaDestructor) {
        m_current->value.customFunction->addCode(ch);
        return true;
    }

    if (m_current->type == StackElement::ConversionRule
        && m_current->parent->type == StackElement::ModifyArgument) {
        m_contextStack.top()->functionMods.last().argument_mods().last().conversionRules().last().addCode(ch);
        return true;
    }

    if (m_current->type == StackElement::NativeToTarget || m_current->type == StackElement::AddConversion) {
       m_contextStack.top()->codeSnips.last().addCode(ch);
       return true;
    }

    if (m_current->parent) {
        if ((m_current->type & StackElement::CodeSnipMask)) {
            CodeSnipList snips;
            switch (m_current->parent->type) {
            case StackElement::Root:
                snips = m_current->parent->entry->codeSnips();
                snips.last().addCode(ch);
                m_current->parent->entry->setCodeSnips(snips);
                break;
            case StackElement::ModifyFunction:
            case StackElement::AddFunction:
                m_contextStack.top()->functionMods.last().snips().last().addCode(ch);
                m_contextStack.top()->functionMods.last().setModifierFlag(FunctionModification::CodeInjection);
                break;
            case StackElement::NamespaceTypeEntry:
            case StackElement::ObjectTypeEntry:
            case StackElement::ValueTypeEntry:
            case StackElement::InterfaceTypeEntry:
                m_contextStack.top()->codeSnips.last().addCode(ch);
                break;
            default:
                Q_ASSERT(false);
            }
            return true;
        }
    }

    if (m_current->type & StackElement::DocumentationMask)
        m_contextStack.top()->docModifications.last().setCode(ch);

    return true;
}

bool TypeSystemParser::importFileElement(const QXmlStreamAttributes &atts)
{
    const QString fileName = atts.value(nameAttribute()).toString();
    if (fileName.isEmpty()) {
        m_error = QLatin1String("Required attribute 'name' missing for include-file tag.");
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.setFileName(QLatin1String(":/trolltech/generator/") + fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_error = msgCannotOpenForReading(file);
            return false;
        }
    }

    const auto quoteFrom = atts.value(quoteAfterLineAttribute());
    bool foundFromOk = quoteFrom.isEmpty();
    bool from = quoteFrom.isEmpty();

    const auto quoteTo = atts.value(quoteBeforeLineAttribute());
    bool foundToOk = quoteTo.isEmpty();
    bool to = true;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (from && to && line.contains(quoteTo)) {
            to = false;
            foundToOk = true;
            break;
        }
        if (from && to)
            characters(line + QLatin1Char('\n'));
        if (!from && line.contains(quoteFrom)) {
            from = true;
            foundFromOk = true;
        }
    }
    if (!foundFromOk || !foundToOk) {
        QString fromError = QStringLiteral("Could not find quote-after-line='%1' in file '%2'.")
                                           .arg(quoteFrom.toString(), fileName);
        QString toError = QStringLiteral("Could not find quote-before-line='%1' in file '%2'.")
                                         .arg(quoteTo.toString(), fileName);

        if (!foundToOk)
            m_error = toError;
        if (!foundFromOk)
            m_error = fromError;
        if (!foundFromOk && !foundToOk)
            m_error = fromError + QLatin1Char(' ') + toError;
        return false;
    }

    return true;
}

static bool convertBoolean(QStringView value, const QString &attributeName, bool defaultValue)
{
    if (value.compare(trueAttributeValue(), Qt::CaseInsensitive) == 0
        || value.compare(yesAttributeValue(), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (value.compare(falseAttributeValue(), Qt::CaseInsensitive) == 0
        || value.compare(noAttributeValue(), Qt::CaseInsensitive) == 0) {
        return false;
    }
    const QString warn = QStringLiteral("Boolean value '%1' not supported in attribute '%2'. Use 'yes' or 'no'. Defaulting to '%3'.")
                                      .arg(value)
                                      .arg(attributeName,
                                           defaultValue ? yesAttributeValue() : noAttributeValue());

    qCWarning(lcShiboken).noquote().nospace() << warn;
    return defaultValue;
}

static bool convertRemovalAttribute(QStringView value)
{
    return value == u"all" // Legacy
        || convertBoolean(value, removeAttribute(), false);
}

// Check whether an entry should be dropped, allowing for dropping the module
// name (match 'Class' and 'Module.Class').
static bool shouldDropTypeEntry(const TypeDatabase *db,
                                const StackElement *element,
                                QString name)
{
    for (auto e = element->parent; e ; e = e->parent) {
        if (e->entry) {
            if (e->entry->type() == TypeEntry::TypeSystemType) {
                if (db->shouldDropTypeEntry(name)) // Unqualified
                    return true;
            }
            name.prepend(QLatin1Char('.'));
            name.prepend(e->entry->name());
        }
    }
    return db->shouldDropTypeEntry(name);
}

// Returns empty string if there's no error.
static QString checkSignatureError(const QString& signature, const QString& tag)
{
    QString funcName = signature.left(signature.indexOf(QLatin1Char('('))).trimmed();
    static const QRegularExpression whiteSpace(QStringLiteral("\\s"));
    Q_ASSERT(whiteSpace.isValid());
    if (!funcName.startsWith(QLatin1String("operator ")) && funcName.contains(whiteSpace)) {
        return QString::fromLatin1("Error in <%1> tag signature attribute '%2'.\n"
                                   "White spaces aren't allowed in function names, "
                                   "and return types should not be part of the signature.")
                                   .arg(tag, signature);
    }
    return QString();
}

inline const TypeEntry *TypeSystemParser::currentParentTypeEntry() const
{
    return m_current ? m_current->entry : nullptr;
}

bool TypeSystemParser::checkRootElement()
{
    const bool ok = currentParentTypeEntry() != nullptr;
    if (!ok)
        m_error = msgNoRootTypeSystemEntry();
    return ok;
}

static TypeEntry *findViewedType(const QString &name)
{
    const auto range = TypeDatabase::instance()->entries().equal_range(name);
    for (auto i = range.first; i != range.second; ++i) {
        switch (i.value()->type()) {
        case TypeEntry::BasicValueType:
        case TypeEntry::PrimitiveType:
        case TypeEntry::ContainerType:
        case TypeEntry::ObjectType:
            return i.value();
        default:
            break;
        }
    }
    return nullptr;
}

bool TypeSystemParser::applyCommonAttributes(const QXmlStreamReader &reader, TypeEntry *type,
                                             QXmlStreamAttributes *attributes)
{
    type->setSourceLocation(SourceLocation(m_currentFile,
                                           reader.lineNumber()));
    type->setCodeGeneration(m_generate);
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name ==  u"revision") {
            type->setRevision(attributes->takeAt(i).value().toInt());
        } else if (name == u"view-on") {
            const QString name = attributes->takeAt(i).value().toString();
            TypeEntry *views = findViewedType(name);
            if (views == nullptr) {
                m_error = msgCannotFindView(name, type->name());
                return false;
            }
            type->setViewOn(views);
        }
    }
    return true;
}

FlagsTypeEntry *
    TypeSystemParser::parseFlagsEntry(const QXmlStreamReader &reader,
                             EnumTypeEntry *enumEntry, QString flagName,
                             const QVersionNumber &since,
                             QXmlStreamAttributes *attributes)

{
    if (!checkRootElement())
        return nullptr;
    auto ftype = new FlagsTypeEntry(QLatin1String("QFlags<") + enumEntry->name() + QLatin1Char('>'),
                                    since,
                                    currentParentTypeEntry()->typeSystemTypeEntry());
    ftype->setOriginator(enumEntry);
    ftype->setTargetLangPackage(enumEntry->targetLangPackage());
    // Try toenumEntry get the guess the qualified flag name
    if (!flagName.contains(colonColon())) {
        auto eq = enumEntry->qualifier();
        if (!eq.isEmpty())
            flagName.prepend(eq + colonColon());
    }

    ftype->setOriginalName(flagName);
    if (!applyCommonAttributes(reader, ftype, attributes))
        return nullptr;

    QStringList lst = flagName.split(colonColon());
    const QString targetLangFlagName = QStringList(lst.mid(0, lst.size() - 1)).join(QLatin1Char('.'));
    const QString &targetLangQualifier = enumEntry->targetLangQualifier();
    if (targetLangFlagName != targetLangQualifier) {
        qCWarning(lcShiboken).noquote().nospace()
            << QStringLiteral("enum %1 and flags %2 (%3) differ in qualifiers")
                              .arg(targetLangQualifier, lst.constFirst(), targetLangFlagName);
    }

    ftype->setFlagsName(lst.constLast());
    enumEntry->setFlags(ftype);

    m_database->addFlagsType(ftype);
    m_database->addType(ftype);

    const int revisionIndex =
        indexOfAttribute(*attributes, u"flags-revision");
    ftype->setRevision(revisionIndex != -1
                       ? attributes->takeAt(revisionIndex).value().toInt()
                       : enumEntry->revision());
    return ftype;
}

SmartPointerTypeEntry *
    TypeSystemParser::parseSmartPointerEntry(const QXmlStreamReader &reader,
                                    const QString &name, const QVersionNumber &since,
                                    QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;
    QString smartPointerType;
    QString getter;
    QString refCountMethodName;
    QString instantiations;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("type")) {
             smartPointerType = attributes->takeAt(i).value().toString();
        } else if (name == QLatin1String("getter")) {
            getter = attributes->takeAt(i).value().toString();
        } else if (name == QLatin1String("ref-count-method")) {
            refCountMethodName = attributes->takeAt(i).value().toString();
        } else if (name == QLatin1String("instantiations")) {
            instantiations = attributes->takeAt(i).value().toString();
        }
    }

    if (smartPointerType.isEmpty()) {
        m_error = QLatin1String("No type specified for the smart pointer. Currently supported types: 'shared',");
        return nullptr;
    }
    if (smartPointerType != QLatin1String("shared")) {
        m_error = QLatin1String("Currently only the 'shared' type is supported.");
        return nullptr;
    }

    if (getter.isEmpty()) {
        m_error = QLatin1String("No function getter name specified for getting the raw pointer held by the smart pointer.");
        return nullptr;
    }

    QString signature = getter + QLatin1String("()");
    signature = TypeDatabase::normalizedSignature(signature);
    if (signature.isEmpty()) {
        m_error = QLatin1String("No signature for the smart pointer getter found.");
        return nullptr;
    }

    QString errorString = checkSignatureError(signature,
                                              QLatin1String("smart-pointer-type"));
    if (!errorString.isEmpty()) {
        m_error = errorString;
        return nullptr;
    }

    auto *type = new SmartPointerTypeEntry(name, getter, smartPointerType,
                                           refCountMethodName, since, currentParentTypeEntry());
    if (!applyCommonAttributes(reader, type, attributes))
        return nullptr;
    m_smartPointerInstantiations.insert(type, instantiations);
    return type;
}

PrimitiveTypeEntry *
    TypeSystemParser::parsePrimitiveTypeEntry(const QXmlStreamReader &reader,
                                     const QString &name, const QVersionNumber &since,
                                     QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;
    auto *type = new PrimitiveTypeEntry(name, since, currentParentTypeEntry());
    if (!applyCommonAttributes(reader, type, attributes))
        return nullptr;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == targetLangNameAttribute()) {
            type->setTargetLangName(attributes->takeAt(i).value().toString());
        } else if (name == QLatin1String("target-lang-api-name")) {
            type->setTargetLangApiName(attributes->takeAt(i).value().toString());
        } else if (name == preferredConversionAttribute()) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
        } else if (name == preferredTargetLangTypeAttribute()) {
            const bool v = convertBoolean(attributes->takeAt(i).value(),
                                          preferredTargetLangTypeAttribute(), true);
            type->setPreferredTargetLangType(v);
        } else if (name == QLatin1String("default-constructor")) {
             type->setDefaultConstructor(attributes->takeAt(i).value().toString());
        }
    }

    if (type->targetLangApiName().isEmpty())
        type->setTargetLangApiName(type->name());
    type->setTargetLangPackage(m_defaultPackage);
    return type;
}

ContainerTypeEntry *
    TypeSystemParser::parseContainerTypeEntry(const QXmlStreamReader &reader,
                                     const QString &name, const QVersionNumber &since,
                                     QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;
    const int typeIndex = indexOfAttribute(*attributes, u"type");
    if (typeIndex == -1) {
        m_error = QLatin1String("no 'type' attribute specified");
        return nullptr;
    }
    const auto typeName = attributes->takeAt(typeIndex).value();
    const auto containerTypeOpt = containerTypeFromAttribute(typeName);
    if (!containerTypeOpt.has_value()) {
        m_error = QLatin1String("there is no container of type ") + typeName.toString();
        return nullptr;
    }
    auto *type = new ContainerTypeEntry(name, containerTypeOpt.value(),
                                        since, currentParentTypeEntry());
    if (!applyCommonAttributes(reader, type, attributes))
        return nullptr;
    return type;
}

EnumTypeEntry *
    TypeSystemParser::parseEnumTypeEntry(const QXmlStreamReader &reader,
                                const QString &name, const QVersionNumber &since,
                                QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;
    auto *entry = new EnumTypeEntry(name, since, currentParentTypeEntry());
    applyCommonAttributes(reader, entry, attributes);
    entry->setTargetLangPackage(m_defaultPackage);

    QString flagNames;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("upper-bound")) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
        } else if (name == QLatin1String("lower-bound")) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
        } else if (name == forceIntegerAttribute()) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
        } else if (name == extensibleAttribute()) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
        } else if (name == flagsAttribute()) {
            flagNames = attributes->takeAt(i).value().toString();
        }
    }

    // put in the flags parallel...
    if (!flagNames.isEmpty()) {
        const QStringList &flagNameList = flagNames.split(QLatin1Char(','));
        for (const QString &flagName : flagNameList)
            parseFlagsEntry(reader, entry, flagName.trimmed(), since, attributes);
    }
    return entry;
}


NamespaceTypeEntry *
    TypeSystemParser::parseNamespaceTypeEntry(const QXmlStreamReader &reader,
                                     const QString &name, const QVersionNumber &since,
                                     QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;
    std::unique_ptr<NamespaceTypeEntry> result(new NamespaceTypeEntry(name, since, currentParentTypeEntry()));
    auto visibility = TypeSystem::Visibility::Unspecified;
    applyCommonAttributes(reader, result.get(), attributes);
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto attributeName = attributes->at(i).qualifiedName();
        if (attributeName == QLatin1String("files")) {
            const QString pattern = attributes->takeAt(i).value().toString();
            QRegularExpression re(pattern);
            if (!re.isValid()) {
                m_error = msgInvalidRegularExpression(pattern, re.errorString());
                return nullptr;
            }
            result->setFilePattern(re);
        } else if (attributeName == QLatin1String("extends")) {
            const auto extendsPackageName = attributes->takeAt(i).value();
            auto allEntries = TypeDatabase::instance()->findNamespaceTypes(name);
            auto extendsIt = std::find_if(allEntries.cbegin(), allEntries.cend(),
                                          [extendsPackageName] (const NamespaceTypeEntry *e) {
                                              return e->targetLangPackage() == extendsPackageName;
                                          });
            if (extendsIt == allEntries.cend()) {
                m_error = msgCannotFindNamespaceToExtend(name, extendsPackageName.toString());
                return nullptr;
            }
            result->setExtends(*extendsIt);
        } else if (attributeName == visibleAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto visibilityOpt = visibilityFromAttribute(attribute.value());
            if (!visibilityOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return nullptr;
            }
            visibility = visibilityOpt.value();
        } else if (attributeName == generateAttribute()) {
            if (!convertBoolean(attributes->takeAt(i).value(), generateAttribute(), true))
                visibility = TypeSystem::Visibility::Invisible;
        } else if (attributeName == generateUsingAttribute()) {
            result->setGenerateUsing(convertBoolean(attributes->takeAt(i).value(), generateUsingAttribute(), true));
        }
    }

    if (visibility != TypeSystem::Visibility::Unspecified)
        result->setVisibility(visibility);
    // Handle legacy "generate" before the common handling
    applyComplexTypeAttributes(reader, result.get(), attributes);

    if (result->extends() && !result->hasPattern()) {
        m_error = msgExtendingNamespaceRequiresPattern(name);
        return nullptr;
    }

    return result.release();
}

ValueTypeEntry *
    TypeSystemParser::parseValueTypeEntry(const QXmlStreamReader &reader,
                                 const QString &name, const QVersionNumber &since,
                                 QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;
    auto *typeEntry = new ValueTypeEntry(name, since, currentParentTypeEntry());
    applyCommonAttributes(reader, typeEntry, attributes);
    const int defaultCtIndex =
        indexOfAttribute(*attributes, u"default-constructor");
    if (defaultCtIndex != -1)
         typeEntry->setDefaultConstructor(attributes->takeAt(defaultCtIndex).value().toString());
    return typeEntry;
}

FunctionTypeEntry *
    TypeSystemParser::parseFunctionTypeEntry(const QXmlStreamReader &reader,
                                    const QString &name, const QVersionNumber &since,
                                    QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;

    QString signature;
    TypeSystem::SnakeCase snakeCase = TypeSystem::SnakeCase::Disabled;

    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == signatureAttribute()) {
            signature = TypeDatabase::normalizedSignature(attributes->takeAt(i).value().toString());
        } else if (name == snakeCaseAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto snakeCaseOpt = snakeCaseFromAttribute(attribute.value());
            if (!snakeCaseOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return nullptr;
            }
            snakeCase = snakeCaseOpt.value();
        }
    }

    if (signature.isEmpty()) {
        m_error =  msgMissingAttribute(signatureAttribute());
        return nullptr;
    }

    TypeEntry *existingType = m_database->findType(name);

    if (!existingType) {
        auto *result = new FunctionTypeEntry(name, signature, since, currentParentTypeEntry());
        result->setSnakeCase(snakeCase);
        applyCommonAttributes(reader, result, attributes);
        return result;
    }

    if (existingType->type() != TypeEntry::FunctionType) {
        m_error = QStringLiteral("%1 expected to be a function, but isn't! Maybe it was already declared as a class or something else.")
                 .arg(name);
        return nullptr;
    }

    auto *result = reinterpret_cast<FunctionTypeEntry *>(existingType);
    result->addSignature(signature);
    return result;
}

TypedefEntry *
 TypeSystemParser::parseTypedefEntry(const QXmlStreamReader &reader,
                                     const QString &name,
                                     const QVersionNumber &since,
                                     QXmlStreamAttributes *attributes)
{
    if (!checkRootElement())
        return nullptr;
    if (m_current && m_current->type != StackElement::Root
        && m_current->type != StackElement::NamespaceTypeEntry) {
        m_error = QLatin1String("typedef entries must be nested in namespaces or type system.");
        return nullptr;
    }
    const int sourceIndex = indexOfAttribute(*attributes, sourceAttribute());
    if (sourceIndex == -1) {
        m_error =  msgMissingAttribute(sourceAttribute());
        return nullptr;
    }
    const QString sourceType = attributes->takeAt(sourceIndex).value().toString();
    auto result = new TypedefEntry(name, sourceType, since, currentParentTypeEntry());
    applyCommonAttributes(reader, result, attributes);
    return result;
}

void TypeSystemParser::applyComplexTypeAttributes(const QXmlStreamReader &reader,
                                         ComplexTypeEntry *ctype,
                                         QXmlStreamAttributes *attributes) const
{
    bool generate = true;
    ctype->setCopyable(ComplexTypeEntry::Unknown);
    auto exceptionHandling = m_exceptionHandling;
    auto allowThread = m_allowThread;

    QString package = m_defaultPackage;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == streamAttribute()) {
            ctype->setStream(convertBoolean(attributes->takeAt(i).value(), streamAttribute(), false));
        } else if (name == generateAttribute()) {
            generate = convertBoolean(attributes->takeAt(i).value(), generateAttribute(), true);
        } else if (name ==packageAttribute()) {
            package = attributes->takeAt(i).value().toString();
        } else if (name == defaultSuperclassAttribute()) {
            ctype->setDefaultSuperclass(attributes->takeAt(i).value().toString());
        } else if (name == genericClassAttribute()) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
            const bool v = convertBoolean(attributes->takeAt(i).value(), genericClassAttribute(), false);
            ctype->setGenericClass(v);
        } else if (name == targetLangNameAttribute()) {
            ctype->setTargetLangName(attributes->takeAt(i).value().toString());
        } else if (name == QLatin1String("polymorphic-base")) {
            ctype->setPolymorphicIdValue(attributes->takeAt(i).value().toString());
        } else if (name == QLatin1String("polymorphic-id-expression")) {
            ctype->setPolymorphicIdValue(attributes->takeAt(i).value().toString());
        } else if (name == copyableAttribute()) {
            const bool v = convertBoolean(attributes->takeAt(i).value(), copyableAttribute(), false);
            ctype->setCopyable(v ? ComplexTypeEntry::CopyableSet : ComplexTypeEntry::NonCopyableSet);
        } else if (name == exceptionHandlingAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto exceptionOpt = exceptionHandlingFromAttribute(attribute.value());
            if (exceptionOpt.has_value()) {
                exceptionHandling = exceptionOpt.value();
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        } else if (name == allowThreadAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto allowThreadOpt = allowThreadFromAttribute(attribute.value());
            if (allowThreadOpt.has_value()) {
                allowThread = allowThreadOpt.value();
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        } else if (name == QLatin1String("held-type")) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
        } else if (name == QLatin1String("hash-function")) {
            ctype->setHashFunction(attributes->takeAt(i).value().toString());
        } else if (name == forceAbstractAttribute()) {
            if (convertBoolean(attributes->takeAt(i).value(), forceAbstractAttribute(), false))
                ctype->setTypeFlags(ctype->typeFlags() | ComplexTypeEntry::ForceAbstract);
        } else if (name == deprecatedAttribute()) {
            if (convertBoolean(attributes->takeAt(i).value(), deprecatedAttribute(), false))
                ctype->setTypeFlags(ctype->typeFlags() | ComplexTypeEntry::Deprecated);
        } else if (name == disableWrapperAttribute()) {
            if (convertBoolean(attributes->takeAt(i).value(), disableWrapperAttribute(), false))
                ctype->setTypeFlags(ctype->typeFlags() | ComplexTypeEntry::DisableWrapper);
        } else if (name == deleteInMainThreadAttribute()) {
            if (convertBoolean(attributes->takeAt(i).value(), deleteInMainThreadAttribute(), false))
                ctype->setDeleteInMainThread(true);
        } else if (name == QLatin1String("target-type")) {
            ctype->setTargetType(attributes->takeAt(i).value().toString());
        }  else if (name == snakeCaseAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto snakeCaseOpt = snakeCaseFromAttribute(attribute.value());
            if (snakeCaseOpt.has_value()) {
                ctype->setSnakeCase(snakeCaseOpt.value());
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        }
    }

    if (exceptionHandling != TypeSystem::ExceptionHandling::Unspecified)
         ctype->setExceptionHandling(exceptionHandling);
    if (allowThread != TypeSystem::AllowThread::Unspecified)
        ctype->setAllowThread(allowThread);

    // The generator code relies on container's package being empty.
    if (ctype->type() != TypeEntry::ContainerType)
        ctype->setTargetLangPackage(package);

    if (generate)
        ctype->setCodeGeneration(m_generate);
    else
        ctype->setCodeGeneration(TypeEntry::GenerationDisabled);
}

bool TypeSystemParser::parseRenameFunction(const QXmlStreamReader &,
                                  QString *name, QXmlStreamAttributes *attributes)
{
    QString signature;
    QString rename;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == signatureAttribute()) {
            // Do not remove as it is needed for the type entry later on
            signature = attributes->at(i).value().toString();
        } else if (name == renameAttribute()) {
            rename = attributes->takeAt(i).value().toString();
        }
    }

    if (signature.isEmpty()) {
        m_error = msgMissingAttribute(signatureAttribute());
        return false;
    }

    *name = signature.left(signature.indexOf(QLatin1Char('('))).trimmed();

    QString errorString = checkSignatureError(signature, QLatin1String("function"));
    if (!errorString.isEmpty()) {
        m_error = errorString;
        return false;
    }

    if (!rename.isEmpty()) {
        static const QRegularExpression functionNameRegExp(QLatin1String("^[a-zA-Z_][a-zA-Z0-9_]*$"));
        Q_ASSERT(functionNameRegExp.isValid());
        if (!functionNameRegExp.match(rename).hasMatch()) {
            m_error = QLatin1String("can not rename '") + signature + QLatin1String("', '")
                      + rename + QLatin1String("' is not a valid function name");
            return false;
        }
        FunctionModification mod;
        if (!mod.setSignature(signature, &m_error))
            return false;
        mod.setRenamedToName(rename);
        mod.setModifierFlag(FunctionModification::Rename);
        m_contextStack.top()->functionMods << mod;
    }
    return true;
}

bool TypeSystemParser::parseInjectDocumentation(const QXmlStreamReader &,
                                       QXmlStreamAttributes *attributes)
{
    const int validParent = StackElement::TypeEntryMask
                            | StackElement::ModifyFunction
                            | StackElement::ModifyField;
    if (!m_current->parent || (m_current->parent->type & validParent) == 0) {
        m_error = QLatin1String("inject-documentation must be inside modify-function, "
                                "modify-field or other tags that creates a type");
        return false;
    }

    TypeSystem::DocModificationMode mode = TypeSystem::DocModificationReplace;
    TypeSystem::Language lang = TypeSystem::NativeCode;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("mode")) {
            const auto attribute = attributes->takeAt(i);
            const auto modeOpt = docModificationFromAttribute(attribute.value());
            if (!modeOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
            mode = modeOpt.value();
        } else if (name == formatAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto langOpt = languageFromAttribute(attribute.value());
            if (!langOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
            lang = langOpt.value();
        }
    }

    QString signature = m_current->type & StackElement::TypeEntryMask
        ? QString() : m_currentSignature;
    DocModification mod(mode, signature);
    mod.setFormat(lang);
    m_contextStack.top()->docModifications << mod;
    return true;
}

bool TypeSystemParser::parseModifyDocumentation(const QXmlStreamReader &,
                                       QXmlStreamAttributes *attributes)
{
    const int validParent = StackElement::TypeEntryMask
                            | StackElement::ModifyFunction
                            | StackElement::ModifyField;
    if (!m_current->parent || (m_current->parent->type & validParent) == 0) {
        m_error = QLatin1String("modify-documentation must be inside modify-function, "
                                "modify-field or other tags that creates a type");
        return false;
    }

    const int xpathIndex = indexOfAttribute(*attributes, xPathAttribute());
    if (xpathIndex == -1) {
        m_error = msgMissingAttribute(xPathAttribute());
        return false;
    }

    const QString xpath = attributes->takeAt(xpathIndex).value().toString();
    QString signature = (m_current->type & StackElement::TypeEntryMask) ? QString() : m_currentSignature;
    m_contextStack.top()->docModifications
        << DocModification(xpath, signature);
    return true;
}

// m_exceptionHandling
TypeSystemTypeEntry *TypeSystemParser::parseRootElement(const QXmlStreamReader &,
                                               const QVersionNumber &since,
                                               QXmlStreamAttributes *attributes)
{
    TypeSystem::SnakeCase snakeCase = TypeSystem::SnakeCase::Unspecified;

    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == packageAttribute()) {
            m_defaultPackage = attributes->takeAt(i).value().toString();
        } else if (name == defaultSuperclassAttribute()) {
            m_defaultSuperclass = attributes->takeAt(i).value().toString();
        } else if (name == exceptionHandlingAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto exceptionOpt = exceptionHandlingFromAttribute(attribute.value());
            if (exceptionOpt.has_value()) {
                m_exceptionHandling = exceptionOpt.value();
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        } else if (name == allowThreadAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto allowThreadOpt = allowThreadFromAttribute(attribute.value());
            if (allowThreadOpt.has_value()) {
                m_allowThread = allowThreadOpt.value();
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        } else if (name == snakeCaseAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto snakeCaseOpt = snakeCaseFromAttribute(attribute.value());
            if (snakeCaseOpt.has_value()) {
                snakeCase = snakeCaseOpt.value();
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        }
    }

    auto *moduleEntry =
        const_cast<TypeSystemTypeEntry *>(m_database->findTypeSystemType(m_defaultPackage));
    const bool add = moduleEntry == nullptr;
    if (add) {
        moduleEntry = new TypeSystemTypeEntry(m_defaultPackage, since,
                                              currentParentTypeEntry());
    }
    moduleEntry->setCodeGeneration(m_generate);
    moduleEntry->setSnakeCase(snakeCase);

    if ((m_generate == TypeEntry::GenerateForSubclass ||
         m_generate == TypeEntry::GenerateNothing) && !m_defaultPackage.isEmpty())
        TypeDatabase::instance()->addRequiredTargetImport(m_defaultPackage);

    if (add)
        m_database->addTypeSystemType(moduleEntry);
    return moduleEntry;
}

bool TypeSystemParser::loadTypesystem(const QXmlStreamReader &,
                             QXmlStreamAttributes *attributes)
{
    QString typeSystemName;
    bool generateChild = true;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == nameAttribute())
            typeSystemName = attributes->takeAt(i).value().toString();
        else if (name == generateAttribute())
           generateChild = convertBoolean(attributes->takeAt(i).value(), generateAttribute(), true);
    }
    if (typeSystemName.isEmpty()) {
            m_error = QLatin1String("No typesystem name specified");
            return false;
    }
    const bool result =
        m_database->parseFile(typeSystemName, m_currentPath, generateChild
                              && m_generate == TypeEntry::GenerateCode);
    if (!result)
        m_error = QStringLiteral("Failed to parse: '%1'").arg(typeSystemName);
    return result;
}

bool TypeSystemParser::parseRejectEnumValue(const QXmlStreamReader &,
                                   QXmlStreamAttributes *attributes)
{
    if (!m_currentEnum) {
        m_error = QLatin1String("<reject-enum-value> node must be used inside a <enum-type> node");
        return false;
    }
    const int nameIndex = indexOfAttribute(*attributes, nameAttribute());
    if (nameIndex == -1) {
        m_error = msgMissingAttribute(nameAttribute());
        return false;
    }
    m_currentEnum->addEnumValueRejection(attributes->takeAt(nameIndex).value().toString());
    return true;
}

bool TypeSystemParser::parseReplaceArgumentType(const QXmlStreamReader &,
                                       const StackElement &topElement,
                                       QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyArgument) {
        m_error = QLatin1String("Type replacement can only be specified for argument modifications");
        return false;
    }
    const int modifiedTypeIndex = indexOfAttribute(*attributes, modifiedTypeAttribute());
    if (modifiedTypeIndex == -1) {
        m_error = QLatin1String("Type replacement requires 'modified-type' attribute");
        return false;
    }
    m_contextStack.top()->functionMods.last().argument_mods().last().setModifiedType(
        attributes->takeAt(modifiedTypeIndex).value().toString());
    return true;
}

bool TypeSystemParser::parseCustomConversion(const QXmlStreamReader &,
                                    const StackElement &topElement,
                                    QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyArgument
        && topElement.type != StackElement::ValueTypeEntry
        && topElement.type != StackElement::PrimitiveTypeEntry
        && topElement.type != StackElement::ContainerTypeEntry) {
        m_error = QLatin1String("Conversion rules can only be specified for argument modification, "
                                "value-type, primitive-type or container-type conversion.");
        return false;
    }

    QString sourceFile;
    QString snippetLabel;
    TypeSystem::Language lang = TypeSystem::NativeCode;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == classAttribute()) {
            const auto languageAttribute = attributes->takeAt(i);
            const auto langOpt = languageFromAttribute(languageAttribute.value());
            if (!langOpt.has_value()) {
                m_error = msgInvalidAttributeValue(languageAttribute);
                return false;
            }
            lang = langOpt.value();
        } else if (name == QLatin1String("file")) {
            sourceFile = attributes->takeAt(i).value().toString();
        } else if (name == snippetAttribute()) {
            snippetLabel = attributes->takeAt(i).value().toString();
        }
    }

    if (topElement.type == StackElement::ModifyArgument) {
        CodeSnip snip;
        snip.language = lang;
        m_contextStack.top()->functionMods.last().argument_mods().last().conversionRules().append(snip);
        return true;
    }

    if (topElement.entry->hasTargetConversionRule() || topElement.entry->hasCustomConversion()) {
        m_error = QLatin1String("Types can have only one conversion rule");
        return false;
    }

    // The old conversion rule tag that uses a file containing the conversion
    // will be kept temporarily for compatibility reasons. FIXME PYSIDE7: Remove
    if (!sourceFile.isEmpty()) {
        if (m_generate != TypeEntry::GenerateForSubclass
                && m_generate != TypeEntry::GenerateNothing) {
            qWarning(lcShiboken, "Specifying conversion rules by \"file\" is deprecated.");
            if (lang != TypeSystem::TargetLangCode)
                return true;

            QFile conversionSource(sourceFile);
            if (!conversionSource.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_error = msgCannotOpenForReading(conversionSource);
                return false;
            }
            const auto conversionRuleOptional =
                extractSnippet(QString::fromUtf8(conversionSource.readAll()), snippetLabel);
            if (!conversionRuleOptional.has_value()) {
                m_error = msgCannotFindSnippet(sourceFile, snippetLabel);
                return false;
            }
            topElement.entry->setTargetConversionRule(conversionRuleOptional.value());
        }
        return true;
    }

    auto *customConversion = new CustomConversion(m_current->entry);
    customConversionsForReview.append(customConversion);
    return true;
}

bool TypeSystemParser::parseNativeToTarget(const QXmlStreamReader &,
                                  const StackElement &topElement,
                                  QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ConversionRule) {
        m_error = QLatin1String("Native to Target conversion code can only be specified for custom conversion rules.");
        return false;
    }
    CodeSnip snip;
    if (!readFileSnippet(attributes, &snip))
        return false;
    m_contextStack.top()->codeSnips.append(snip);
    return true;
}

bool TypeSystemParser::parseAddConversion(const QXmlStreamReader &,
                                 const StackElement &topElement,
                                 QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::TargetToNative) {
        m_error = QLatin1String("Target to Native conversions can only be added inside 'target-to-native' tags.");
        return false;
    }
    QString sourceTypeName;
    QString typeCheck;
    CodeSnip snip;
    if (!readFileSnippet(attributes, &snip))
        return false;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("type"))
             sourceTypeName = attributes->takeAt(i).value().toString();
        else if (name == QLatin1String("check"))
           typeCheck = attributes->takeAt(i).value().toString();
    }
    if (sourceTypeName.isEmpty()) {
        m_error = QLatin1String("Target to Native conversions must specify the input type with the 'type' attribute.");
        return false;
    }
    m_current->entry->customConversion()->addTargetToNativeConversion(sourceTypeName, typeCheck);
    m_contextStack.top()->codeSnips.append(snip);
    return true;
}

static bool parseIndex(const QString &index, int *result, QString *errorMessage)
{
    bool ok = false;
    *result = index.toInt(&ok);
    if (!ok)
        *errorMessage = QStringLiteral("Cannot convert '%1' to integer").arg(index);
    return ok;
}

static bool parseArgumentIndex(const QString &index, int *result, QString *errorMessage)
{
    if (index == QLatin1String("return")) {
        *result = 0;
        return true;
    }
    if (index == QLatin1String("this")) {
        *result = -1;
        return true;
    }
    return parseIndex(index, result, errorMessage);
}

bool TypeSystemParser::parseModifyArgument(const QXmlStreamReader &,
                                  const StackElement &topElement, QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyFunction
            && topElement.type != StackElement::AddFunction) {
        m_error = QString::fromLatin1("argument modification requires function"
                                      " modification as parent, was %1")
                                      .arg(topElement.type, 0, 16);
        return false;
    }

    QString index;
    QString renameTo;
    bool resetAfterUse = false;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == indexAttribute()) {
             index = attributes->takeAt(i).value().toString();
        } else if (name == invalidateAfterUseAttribute()) {
            resetAfterUse = convertBoolean(attributes->takeAt(i).value(),
                                           invalidateAfterUseAttribute(), false);
        } else if (name == renameAttribute()) {
            renameTo = attributes->takeAt(i).value().toString();
        }
    }

    if (index.isEmpty()) {
        m_error = msgMissingAttribute(indexAttribute());
        return false;
    }

    int idx;
    if (!parseArgumentIndex(index, &idx, &m_error))
        return false;

    ArgumentModification argumentModification = ArgumentModification(idx);
    argumentModification.setResetAfterUse(resetAfterUse);
    argumentModification.setRenamedToName(renameTo);
    m_contextStack.top()->functionMods.last().argument_mods().append(argumentModification);
    return true;
}

bool TypeSystemParser::parseNoNullPointer(const QXmlStreamReader &reader,
                                 const StackElement &topElement, QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyArgument) {
        m_error = QLatin1String("no-null-pointer requires argument modification as parent");
        return false;
    }

    ArgumentModification &lastArgMod = m_contextStack.top()->functionMods.last().argument_mods().last();
    lastArgMod.setNoNullPointers(true);

    const int defaultValueIndex =
        indexOfAttribute(*attributes, u"default-value");
    if (defaultValueIndex != -1) {
        const QXmlStreamAttribute attribute = attributes->takeAt(defaultValueIndex);
        qCWarning(lcShiboken, "%s",
                  qPrintable(msgUnimplementedAttributeWarning(reader, attribute)));
    }
    return true;
}

bool TypeSystemParser::parseDefineOwnership(const QXmlStreamReader &,
                                   const StackElement &topElement,
                                   QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyArgument) {
        m_error = QLatin1String("define-ownership requires argument modification as parent");
        return false;
    }

    TypeSystem::Language lang = TypeSystem::TargetLangCode;
    std::optional<TypeSystem::Ownership> ownershipOpt;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == classAttribute()) {
            const auto classAttribute = attributes->takeAt(i);
            const auto langOpt = languageFromAttribute(classAttribute.value());
            if (!langOpt.has_value() || langOpt.value() == TypeSystem::ShellCode) {
                m_error = msgInvalidAttributeValue(classAttribute);
                return false;
            }
            lang = langOpt.value();
        } else if (name == ownershipAttribute()) {
            const auto attribute = attributes->takeAt(i);
            ownershipOpt = ownershipFromFromAttribute(attribute.value());
            if (!ownershipOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
        }
    }

    if (!ownershipOpt.has_value()) {
        m_error = QStringLiteral("unspecified ownership");
        return false;
    }
    auto &lastArgMod = m_contextStack.top()->functionMods.last().argument_mods().last();
    switch (lang) {
    case TypeSystem::TargetLangCode:
        lastArgMod.setTargetOwnerShip(ownershipOpt.value());
        break;
    case TypeSystem::NativeCode:
        lastArgMod.setNativeOwnership(ownershipOpt.value());
        break;
    default:
        break;
    }
    return true;
}

// ### fixme PySide7: remove (replaced by attribute).
bool TypeSystemParser::parseRename(const QXmlStreamReader &,
                          const StackElement &topElement,
                          QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyArgument) {
        m_error = QLatin1String("Argument modification parent required");
        return false;
    }

    const int toIndex = indexOfAttribute(*attributes, toAttribute());
    if (toIndex == -1) {
        m_error = msgMissingAttribute(toAttribute());
        return false;
    }
    const QString renamed_to = attributes->takeAt(toIndex).value().toString();
    m_contextStack.top()->functionMods.last().argument_mods().last().setRenamedToName(renamed_to);
    return true;
}

bool TypeSystemParser::parseModifyField(const QXmlStreamReader &,
                                        QXmlStreamAttributes *attributes)
{
    FieldModification fm;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == nameAttribute()) {
            fm.setName(attributes->takeAt(i).value().toString());
        } else if (name == removeAttribute()) {
            fm.setRemoved(convertRemovalAttribute(attributes->takeAt(i).value()));
        }  else if (name == readAttribute()) {
            fm.setReadable(convertBoolean(attributes->takeAt(i).value(), readAttribute(), true));
        } else if (name == writeAttribute()) {
            fm.setWritable(convertBoolean(attributes->takeAt(i).value(), writeAttribute(), true));
        } else if (name == renameAttribute()) {
            fm.setRenamedToName(attributes->takeAt(i).value().toString());
        } else if (name == snakeCaseAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto snakeCaseOpt = snakeCaseFromAttribute(attribute.value());
            if (snakeCaseOpt.has_value()) {
                fm.setSnakeCase(snakeCaseOpt.value());
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        }
    }
    if (fm.name().isEmpty()) {
        m_error = msgMissingAttribute(nameAttribute());
        return false;
    }
    m_contextStack.top()->fieldMods << fm;
    return true;
}

static bool parseOverloadNumber(const QXmlStreamAttribute &attribute, int *overloadNumber,
                                QString *errorMessage)
{
    bool ok;
    *overloadNumber = attribute.value().toInt(&ok);
    if (!ok || *overloadNumber < 0) {
        *errorMessage = msgInvalidAttributeValue(attribute);
        return false;
    }
    return true;
}

bool TypeSystemParser::parseAddFunction(const QXmlStreamReader &,
                                        const StackElement &topElement,
                                        StackElement::ElementType t,
                                        QXmlStreamAttributes *attributes)
{
    if (!(topElement.type
          & (StackElement::ComplexTypeEntryMask | StackElement::Root | StackElement::ContainerTypeEntry))) {
        m_error = QString::fromLatin1("Add/Declare function requires a complex/container type or a root tag as parent"
                                      ", was=%1").arg(topElement.type, 0, 16);
        return false;
    }
    QString originalSignature;
    QString returnType;
    bool staticFunction = false;
    QString access;
    int overloadNumber = TypeSystem::OverloadNumberUnset;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("signature")) {
            originalSignature = attributes->takeAt(i).value().toString();
        } else if (name == QLatin1String("return-type")) {
            returnType = attributes->takeAt(i).value().toString();
        } else if (name == staticAttribute()) {
            staticFunction = convertBoolean(attributes->takeAt(i).value(),
                                            staticAttribute(), false);
        } else if (name == accessAttribute()) {
            access = attributes->takeAt(i).value().toString();
        } else if (name == overloadNumberAttribute()) {
            if (!parseOverloadNumber(attributes->takeAt(i), &overloadNumber, &m_error))
                return false;
        }
    }

    QString signature = TypeDatabase::normalizedSignature(originalSignature);
    if (signature.isEmpty()) {
        m_error = QLatin1String("No signature for the added function");
        return false;
    }

    QString errorString = checkSignatureError(signature, QLatin1String("add-function"));
    if (!errorString.isEmpty()) {
        m_error = errorString;
        return false;
    }

    AddedFunctionPtr func = AddedFunction::createAddedFunction(signature, returnType, &errorString);
    if (func.isNull()) {
        m_error = errorString;
        return false;
    }

    func->setStatic(staticFunction);
    if (!signature.contains(QLatin1Char('(')))
        signature += QLatin1String("()");
    m_currentSignature = signature;

    if (!access.isEmpty()) {
        const auto acessOpt = addedFunctionAccessFromAttribute(access);
        if (!acessOpt.has_value()) {
            m_error = QString::fromLatin1("Bad access type '%1'").arg(access);
            return false;
        }
        func->setAccess(acessOpt.value());
    }
    func->setDeclaration(t == StackElement::DeclareFunction);

    m_contextStack.top()->addedFunctions << func;
    m_contextStack.top()->addedFunctionModificationIndex =
        m_contextStack.top()->functionMods.size();

    FunctionModification mod;
    mod.setOverloadNumber(overloadNumber);
    if (!mod.setSignature(m_currentSignature, &m_error))
        return false;
    mod.setOriginalSignature(originalSignature);
    m_contextStack.top()->functionMods << mod;
    return true;
}

bool TypeSystemParser::parseProperty(const QXmlStreamReader &, const StackElement &topElement,
                                     QXmlStreamAttributes *attributes)
{
    if ((topElement.type & StackElement::ComplexTypeEntryMask) == 0) {
        m_error = QString::fromLatin1("Add property requires a complex type as parent"
                                      ", was=%1").arg(topElement.type, 0, 16);
        return false;
    }

    TypeSystemProperty property;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == nameAttribute()) {
            property.name = attributes->takeAt(i).value().toString();
        } else if (name == QLatin1String("get")) {
            property.read = attributes->takeAt(i).value().toString();
        } else if (name == QLatin1String("type")) {
            property.type = attributes->takeAt(i).value().toString();
        } else if (name == QLatin1String("set")) {
            property.write = attributes->takeAt(i).value().toString();
        } else if (name == generateGetSetDefAttribute()) {
            property.generateGetSetDef =
                convertBoolean(attributes->takeAt(i).value(),
                               generateGetSetDefAttribute(), false);
        }
    }
    if (!property.isValid()) {
        m_error = QLatin1String("<property> element is missing required attibutes (name/type/get).");
        return false;
    }
    static_cast<ComplexTypeEntry *>(topElement.entry)->addProperty(property);
    return true;
}

bool TypeSystemParser::parseModifyFunction(const QXmlStreamReader &reader,
                                  const StackElement &topElement,
                                  QXmlStreamAttributes *attributes)
{
    if (!(topElement.type & StackElement::ComplexTypeEntryMask)) {
        m_error = QString::fromLatin1("Modify function requires complex type as parent"
                                      ", was=%1").arg(topElement.type, 0, 16);
        return false;
    }

    QString originalSignature;
    QString access;
    bool removed = false;
    QString rename;
    bool deprecated = false;
    bool isThread = false;
    int overloadNumber = TypeSystem::OverloadNumberUnset;
    TypeSystem::ExceptionHandling exceptionHandling = TypeSystem::ExceptionHandling::Unspecified;
    TypeSystem::AllowThread allowThread = TypeSystem::AllowThread::Unspecified;
    TypeSystem::SnakeCase snakeCase = TypeSystem::SnakeCase::Unspecified;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("signature")) {
            originalSignature = attributes->takeAt(i).value().toString();
        } else if (name == accessAttribute()) {
            access = attributes->takeAt(i).value().toString();
        } else if (name == renameAttribute()) {
            rename = attributes->takeAt(i).value().toString();
        } else if (name == removeAttribute()) {
            removed = convertRemovalAttribute(attributes->takeAt(i).value());
        } else if (name == deprecatedAttribute()) {
            deprecated = convertBoolean(attributes->takeAt(i).value(),
                                        deprecatedAttribute(), false);
        } else if (name == threadAttribute()) {
            isThread = convertBoolean(attributes->takeAt(i).value(),
                                      threadAttribute(), false);
        } else if (name == allowThreadAttribute()) {
            const QXmlStreamAttribute attribute = attributes->takeAt(i);
            const auto allowThreadOpt = allowThreadFromAttribute(attribute.value());
            if (!allowThreadOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
            allowThread = allowThreadOpt.value();
        } else if (name == exceptionHandlingAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto exceptionOpt = exceptionHandlingFromAttribute(attribute.value());
            if (exceptionOpt.has_value()) {
                exceptionHandling = exceptionOpt.value();
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        } else if (name == overloadNumberAttribute()) {
            if (!parseOverloadNumber(attributes->takeAt(i), &overloadNumber, &m_error))
                return false;
        } else if (name == snakeCaseAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto snakeCaseOpt = snakeCaseFromAttribute(attribute.value());
            if (snakeCaseOpt.has_value()) {
                snakeCase = snakeCaseOpt.value();
            } else {
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgInvalidAttributeValue(attribute)));
            }
        } else if (name == virtualSlotAttribute()) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeWarning(reader, name)));
        }
    }

    // Child of global <function>
    if (originalSignature.isEmpty() && topElement.entry->isFunction()) {
        auto f = static_cast<const FunctionTypeEntry *>(topElement.entry);
        originalSignature = f->signatures().value(0);
    }

    const QString signature = TypeDatabase::normalizedSignature(originalSignature);
    if (signature.isEmpty()) {
        m_error = QLatin1String("No signature for modified function");
        return false;
    }

    QString errorString = checkSignatureError(signature, QLatin1String("modify-function"));
    if (!errorString.isEmpty()) {
        m_error = errorString;
        return false;
    }

    FunctionModification mod;
    if (!mod.setSignature(signature, &m_error))
        return false;
    mod.setOriginalSignature(originalSignature);
    mod.setExceptionHandling(exceptionHandling);
    mod.setOverloadNumber(overloadNumber);
    mod.setSnakeCase(snakeCase);
    m_currentSignature = signature;

    if (!access.isEmpty()) {
        const auto modifierFlagOpt = modifierFromAttribute(access);
        if (!modifierFlagOpt.has_value()) {
            m_error = QString::fromLatin1("Bad access type '%1'").arg(access);
            return false;
        }
        const FunctionModification::ModifierFlag m = modifierFlagOpt.value();
        if (m == FunctionModification::Final || m == FunctionModification::NonFinal) {
            qCWarning(lcShiboken, "%s",
                      qPrintable(msgUnimplementedAttributeValueWarning(reader,
                      accessAttribute(), access)));
        }
        mod.setModifierFlag(m);
    }

    if (deprecated)
        mod.setModifierFlag(FunctionModification::Deprecated);

    mod.setRemoved(removed);

    if (!rename.isEmpty()) {
        mod.setRenamedToName(rename);
        mod.setModifierFlag(FunctionModification::Rename);
    }

    mod.setIsThread(isThread);
    if (allowThread != TypeSystem::AllowThread::Unspecified)
        mod.setAllowThread(allowThread);

    m_contextStack.top()->functionMods << mod;
    return true;
}

bool TypeSystemParser::parseReplaceDefaultExpression(const QXmlStreamReader &,
                                            const StackElement &topElement,
                                            QXmlStreamAttributes *attributes)
{
    if (!(topElement.type & StackElement::ModifyArgument)) {
        m_error = QLatin1String("Replace default expression only allowed as child of argument modification");
        return false;
    }
    const int withIndex = indexOfAttribute(*attributes, u"with");
    if (withIndex == -1 || attributes->at(withIndex).value().isEmpty()) {
        m_error = QLatin1String("Default expression replaced with empty string. Use remove-default-expression instead.");
        return false;
    }

    m_contextStack.top()->functionMods.last().argument_mods().last().setReplacedDefaultExpression(
        attributes->takeAt(withIndex).value().toString());
    return true;
}

CustomFunction *
    TypeSystemParser::parseCustomMetaConstructor(const QXmlStreamReader &,
                                        StackElement::ElementType type,
                                        const StackElement &topElement,
                                        QXmlStreamAttributes *attributes)
{
    QString functionName = topElement.entry->name().toLower()
        + (type == StackElement::CustomMetaConstructor
           ? QLatin1String("_create") : QLatin1String("_delete"));
    QString paramName = QLatin1String("copy");
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == nameAttribute())
            functionName = attributes->takeAt(i).value().toString();
        else if (name == QLatin1String("param-name"))
            paramName = attributes->takeAt(i).value().toString();
    }
    auto *func = new CustomFunction(functionName);
    func->paramName = paramName;
    return func;
}

bool TypeSystemParser::parseReferenceCount(const QXmlStreamReader &reader,
                                  const StackElement &topElement,
                                  QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyArgument) {
        m_error = QLatin1String("reference-count must be child of modify-argument");
        return false;
    }

    ReferenceCount rc;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == actionAttribute()) {
            const QXmlStreamAttribute attribute = attributes->takeAt(i);
            const auto actionOpt = referenceCountFromAttribute(attribute.value());
            if (!actionOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
            rc.action = actionOpt.value();
            switch (rc.action) {
            case ReferenceCount::AddAll:
            case ReferenceCount::Ignore:
                qCWarning(lcShiboken, "%s",
                          qPrintable(msgUnimplementedAttributeValueWarning(reader, attribute)));
                break;
            default:
                break;
            }
        } else if (name == QLatin1String("variable-name")) {
            rc.varName = attributes->takeAt(i).value().toString();
        }
    }

    m_contextStack.top()->functionMods.last().argument_mods().last().addReferenceCount(rc);
    return true;
}

bool TypeSystemParser::parseParentOwner(const QXmlStreamReader &,
                               const StackElement &topElement,
                               QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::ModifyArgument) {
        m_error = QLatin1String("parent-policy must be child of modify-argument");
        return false;
    }
    ArgumentOwner ao;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == indexAttribute()) {
            const QString index = attributes->takeAt(i).value().toString();
            if (!parseArgumentIndex(index, &ao.index, &m_error))
                return false;
        } else if (name == actionAttribute()) {
            const auto action = attributes->takeAt(i);
            const auto actionOpt = argumentOwnerActionFromAttribute(action.value());
            if (!actionOpt.has_value()) {
                m_error = msgInvalidAttributeValue(action);
                return false;
            }
            ao.action = actionOpt.value();
        }
    }
    m_contextStack.top()->functionMods.last().argument_mods().last().setOwner(ao);
    return true;
}

bool TypeSystemParser::readFileSnippet(QXmlStreamAttributes *attributes, CodeSnip *snip)
{
    QString fileName;
    QString snippetLabel;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("file")) {
            fileName = attributes->takeAt(i).value().toString();
        } else if (name == snippetAttribute()) {
            snippetLabel = attributes->takeAt(i).value().toString();
        }
    }
    if (fileName.isEmpty())
        return true;
    const QString resolved = m_database->modifiedTypesystemFilepath(fileName, m_currentPath);
    if (!QFile::exists(resolved)) {
        m_error = QLatin1String("File for inject code not exist: ")
            + QDir::toNativeSeparators(fileName);
        return false;
    }
    QFile codeFile(resolved);
    if (!codeFile.open(QIODevice::Text | QIODevice::ReadOnly)) {
        m_error = msgCannotOpenForReading(codeFile);
        return false;
    }
    const auto codeOptional = extractSnippet(QString::fromUtf8(codeFile.readAll()), snippetLabel);
    codeFile.close();
    if (!codeOptional.has_value()) {
        m_error = msgCannotFindSnippet(resolved, snippetLabel);
        return false;
    }

    QString source = fileName;
    if (!snippetLabel.isEmpty())
        source += QLatin1String(" (") + snippetLabel + QLatin1Char(')');
    QString content;
    QTextStream str(&content);
    str << "// ========================================================================\n"
           "// START of custom code block [file: "
        << source << "]\n" << codeOptional.value()
        << "// END of custom code block [file: " << source
        << "]\n// ========================================================================\n";
    snip->addCode(content);
    return true;
}

bool TypeSystemParser::parseInjectCode(const QXmlStreamReader &,
                              const StackElement &topElement,
                              StackElement* element, QXmlStreamAttributes *attributes)
{
    if (!(topElement.type & StackElement::ComplexTypeEntryMask)
        && (topElement.type != StackElement::AddFunction)
        && (topElement.type != StackElement::ModifyFunction)
        && (topElement.type != StackElement::Root)) {
        m_error = QLatin1String("wrong parent type for code injection");
        return false;
    }

    TypeSystem::CodeSnipPosition position = TypeSystem::CodeSnipPositionBeginning;
    TypeSystem::Language lang = TypeSystem::TargetLangCode;
    CodeSnip snip;
    if (!readFileSnippet(attributes, &snip))
        return false;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == classAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto langOpt = languageFromAttribute(attribute.value());
            if (!langOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
            lang = langOpt.value();
        } else if (name == positionAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto positionOpt = codeSnipPositionFromAttribute(attribute.value());
            if (!positionOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
            position = positionOpt.value();
        }
    }

    snip.position = position;
    snip.language = lang;

    if (topElement.type == StackElement::ModifyFunction
        || topElement.type == StackElement::AddFunction) {
        FunctionModification &mod = m_contextStack.top()->functionMods.last();
        mod.appendSnip(snip);
        if (!snip.code().isEmpty())
            mod.setModifierFlag(FunctionModification::CodeInjection);
        element->type = StackElement::InjectCodeInFunction;
    } else if (topElement.type == StackElement::Root) {
        element->entry->addCodeSnip(snip);
    } else if (topElement.type != StackElement::Root) {
        m_contextStack.top()->codeSnips << snip;
    }
    return true;
}

bool TypeSystemParser::parseInclude(const QXmlStreamReader &,
                           const StackElement &topElement,
                           TypeEntry *entry, QXmlStreamAttributes *attributes)
{
    QString fileName;
    Include::IncludeType location = Include::IncludePath;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == fileNameAttribute()) {
            fileName = attributes->takeAt(i).value().toString();
        } else if (name == locationAttribute()) {
            const auto attribute = attributes->takeAt(i);
            const auto locationOpt = locationFromAttribute(attribute.value());
            if (!locationOpt.has_value()) {
                m_error = msgInvalidAttributeValue(attribute);
                return false;
            }
            location = locationOpt.value();
        }
    }

    Include inc(location, fileName);
    if (topElement.type
        & (StackElement::ComplexTypeEntryMask | StackElement::PrimitiveTypeEntry)) {
        entry->setInclude(inc);
    } else if (topElement.type == StackElement::ExtraIncludes) {
        entry->addExtraInclude(inc);
    } else {
        m_error = QLatin1String("Only supported parent tags are primitive-type, complex types or extra-includes");
        return false;
    }
    return true;
}

bool TypeSystemParser::parseSystemInclude(const QXmlStreamReader &,
                                          QXmlStreamAttributes *attributes)
{
    const int index = indexOfAttribute(*attributes, fileNameAttribute());
    if (index == -1) {
        m_error = msgMissingAttribute(fileNameAttribute());
        return false;
    }
    TypeDatabase::instance()->addSystemInclude(attributes->takeAt(index).value().toString());
    return true;
}

TemplateInstance *
    TypeSystemParser::parseTemplateInstanceEnum(const QXmlStreamReader &,
                                       const StackElement &topElement,
                                       QXmlStreamAttributes *attributes)
{
    if (!(topElement.type & StackElement::CodeSnipMask) &&
        (topElement.type != StackElement::Template) &&
        (topElement.type != StackElement::CustomMetaConstructor) &&
        (topElement.type != StackElement::CustomMetaDestructor) &&
        (topElement.type != StackElement::NativeToTarget) &&
        (topElement.type != StackElement::AddConversion) &&
        (topElement.type != StackElement::ConversionRule)) {
        m_error = QLatin1String("Can only insert templates into code snippets, templates, custom-constructors, "\
                                "custom-destructors, conversion-rule, native-to-target or add-conversion tags.");
        return nullptr;
    }
    const int nameIndex = indexOfAttribute(*attributes, nameAttribute());
    if (nameIndex == -1) {
        m_error = msgMissingAttribute(nameAttribute());
        return nullptr;
    }
    return new TemplateInstance(attributes->takeAt(nameIndex).value().toString());
}

bool TypeSystemParser::parseReplace(const QXmlStreamReader &,
                           const StackElement &topElement,
                           StackElement *element, QXmlStreamAttributes *attributes)
{
    if (topElement.type != StackElement::TemplateInstanceEnum) {
        m_error = QLatin1String("Can only insert replace rules into insert-template.");
        return false;
    }
    QString from;
    QString to;
    for (int i = attributes->size() - 1; i >= 0; --i) {
        const auto name = attributes->at(i).qualifiedName();
        if (name == QLatin1String("from"))
            from = attributes->takeAt(i).value().toString();
        else if (name == toAttribute())
            to = attributes->takeAt(i).value().toString();
    }
    element->parent->value.templateInstance->addReplaceRule(from, to);
    return true;
}

static bool parseVersion(const QString &versionSpec, const QString &package,
                         QVersionNumber *result, QString *errorMessage)
{
    *result = QVersionNumber::fromString(versionSpec);
    if (result->isNull()) {
        *errorMessage = msgInvalidVersion(versionSpec, package);
        return false;
    }
    return true;
}

bool TypeSystemParser::startElement(const QXmlStreamReader &reader)
{
    if (m_ignoreDepth) {
        ++m_ignoreDepth;
        return true;
    }

    const auto tagName = reader.name();
    QXmlStreamAttributes attributes = reader.attributes();

    VersionRange versionRange;
    for (int i = attributes.size() - 1; i >= 0; --i) {
        const auto name = attributes.at(i).qualifiedName();
        if (name == sinceAttribute()) {
            if (!parseVersion(attributes.takeAt(i).value().toString(),
                              m_defaultPackage, &versionRange.since, &m_error)) {
                return false;
            }
        } else if (name == untilAttribute()) {
            if (!parseVersion(attributes.takeAt(i).value().toString(),
                              m_defaultPackage, &versionRange.until, &m_error)) {
                return false;
            }
        }
    }

    if (!m_defaultPackage.isEmpty() && !versionRange.isNull()) {
        TypeDatabase* td = TypeDatabase::instance();
        if (!td->checkApiVersion(m_defaultPackage, versionRange)) {
            ++m_ignoreDepth;
            return true;
        }
    }

    if (tagName.compare(QLatin1String("import-file"), Qt::CaseInsensitive) == 0)
        return importFileElement(attributes);

    const auto elementTypeOpt = elementFromTag(tagName);
    if (!elementTypeOpt.has_value()) {
        m_error = QStringLiteral("Unknown tag name: '%1'").arg(tagName);
        return false;
    }

    if (m_currentDroppedEntry) {
        ++m_currentDroppedEntryDepth;
        return true;
    }

    std::unique_ptr<StackElement> element(new StackElement(m_current));
    element->type = elementTypeOpt.value();

    if (element->type == StackElement::Root && m_generate == TypeEntry::GenerateCode)
        customConversionsForReview.clear();

    if (element->type == StackElement::CustomMetaConstructor
        || element->type == StackElement::CustomMetaDestructor) {
        qCWarning(lcShiboken, "%s",
                  qPrintable(msgUnimplementedElementWarning(reader, tagName)));
    }

    switch (element->type) {
    case StackElement::Root:
    case StackElement::NamespaceTypeEntry:
    case StackElement::InterfaceTypeEntry:
    case StackElement::ObjectTypeEntry:
    case StackElement::ValueTypeEntry:
    case StackElement::PrimitiveTypeEntry:
    case StackElement::TypedefTypeEntry:
    case StackElement::ContainerTypeEntry:
        m_contextStack.push(new StackElementContext());
        break;
    default:
        break;
    }

    if (element->type & StackElement::TypeEntryMask) {
        QString name;
        if (element->type != StackElement::FunctionTypeEntry) {
            const int nameIndex = indexOfAttribute(attributes, nameAttribute());
            if (nameIndex != -1) {
                name = attributes.takeAt(nameIndex).value().toString();
            } else if (element->type != StackElement::EnumTypeEntry) { // anonymous enum?
                m_error = msgMissingAttribute(nameAttribute());
                return false;
            }
        }
        // Allow for primitive and/or std:: types only, else require proper nesting.
        if (element->type != StackElement::PrimitiveTypeEntry && name.contains(QLatin1Char(':'))
            && !name.contains(QLatin1String("std::"))) {
            m_error = msgIncorrectlyNestedName(name);
            return false;
        }

        if (m_database->hasDroppedTypeEntries()) {
            const QString identifier = element->type == StackElement::FunctionTypeEntry
                ? attributes.value(signatureAttribute()).toString() : name;
            if (shouldDropTypeEntry(m_database, element.get(), identifier)) {
                m_currentDroppedEntry = element.release();
                m_currentDroppedEntryDepth = 1;
                if (ReportHandler::isDebug(ReportHandler::SparseDebug)) {
                    qCInfo(lcShiboken, "Type system entry '%s' was intentionally dropped from generation.",
                           qPrintable(identifier));
                }
                return true;
            }
        }

        // The top level tag 'function' has only the 'signature' tag
        // and we should extract the 'name' value from it.
        if (element->type == StackElement::FunctionTypeEntry
            && !parseRenameFunction(reader, &name, &attributes)) {
                return false;
        }

        // We need to be able to have duplicate primitive type entries,
        // or it's not possible to cover all primitive target language
        // types (which we need to do in order to support fake meta objects)
        if (element->type != StackElement::PrimitiveTypeEntry
            && element->type != StackElement::FunctionTypeEntry) {
            TypeEntry *tmp = m_database->findType(name);
            if (tmp && !tmp->isNamespace())
                qCWarning(lcShiboken).noquote().nospace()
                    << QStringLiteral("Duplicate type entry: '%1'").arg(name);
        }

        if (element->type == StackElement::EnumTypeEntry) {
            const int enumIdentifiedByIndex = indexOfAttribute(attributes, enumIdentifiedByValueAttribute());
            const QString identifiedByValue = enumIdentifiedByIndex != -1
                ? attributes.takeAt(enumIdentifiedByIndex).value().toString() : QString();
            if (name.isEmpty()) {
                name = identifiedByValue;
            } else if (!identifiedByValue.isEmpty()) {
                m_error = QLatin1String("can't specify both 'name' and 'identified-by-value' attributes");
                return false;
            }
        }

        if (name.isEmpty()) {
            m_error = QLatin1String("no 'name' attribute specified");
            return false;
        }

        switch (element->type) {
        case StackElement::CustomTypeEntry:
            if (!checkRootElement())
                return false;
            element->entry = new TypeEntry(name, TypeEntry::CustomType, versionRange.since, m_current->entry);
            break;
        case StackElement::PrimitiveTypeEntry:
            element->entry = parsePrimitiveTypeEntry(reader, name, versionRange.since, &attributes);
            if (Q_UNLIKELY(!element->entry))
                return false;
            break;
        case StackElement::ContainerTypeEntry:
            if (ContainerTypeEntry *ce = parseContainerTypeEntry(reader, name, versionRange.since, &attributes)) {
                applyComplexTypeAttributes(reader, ce, &attributes);
                element->entry = ce;
            } else {
                return false;
            }
            break;

        case StackElement::SmartPointerTypeEntry:
            if (SmartPointerTypeEntry *se = parseSmartPointerEntry(reader, name, versionRange.since, &attributes)) {
                applyComplexTypeAttributes(reader, se, &attributes);
                element->entry = se;
            } else {
                return false;
            }
            break;
        case StackElement::EnumTypeEntry:
            m_currentEnum = parseEnumTypeEntry(reader, name, versionRange.since, &attributes);
            if (Q_UNLIKELY(!m_currentEnum))
                return false;
            element->entry = m_currentEnum;
            break;

        case StackElement::ValueTypeEntry:
           if (ValueTypeEntry *ve = parseValueTypeEntry(reader, name, versionRange.since, &attributes)) {
               applyComplexTypeAttributes(reader, ve, &attributes);
               element->entry = ve;
           } else {
               return false;
           }
           break;
        case StackElement::NamespaceTypeEntry:
            if (auto entry = parseNamespaceTypeEntry(reader, name, versionRange.since, &attributes))
                element->entry = entry;
            else
                return false;
            break;
        case StackElement::ObjectTypeEntry:
        case StackElement::InterfaceTypeEntry:
            if (!checkRootElement())
                return false;
            element->entry = new ObjectTypeEntry(name, versionRange.since, currentParentTypeEntry());
            applyCommonAttributes(reader, element->entry, &attributes);
            applyComplexTypeAttributes(reader, static_cast<ComplexTypeEntry *>(element->entry), &attributes);
            break;
        case StackElement::FunctionTypeEntry:
            element->entry = parseFunctionTypeEntry(reader, name, versionRange.since, &attributes);
            if (Q_UNLIKELY(!element->entry))
                return false;
            break;
        case StackElement::TypedefTypeEntry:
            if (TypedefEntry *te = parseTypedefEntry(reader, name, versionRange.since, &attributes)) {
                applyComplexTypeAttributes(reader, te, &attributes);
                element->entry = te;
            } else {
                return false;
            }
            break;
        default:
            Q_ASSERT(false);
        }

        if (element->entry) {
            if (!m_database->addType(element->entry, &m_error))
                return false;
        } else {
            qCWarning(lcShiboken).noquote().nospace()
                << QStringLiteral("Type: %1 was rejected by typesystem").arg(name);
        }

    } else if (element->type == StackElement::InjectDocumentation) {
        if (!parseInjectDocumentation(reader, &attributes))
            return false;
    } else if (element->type == StackElement::ModifyDocumentation) {
        if (!parseModifyDocumentation(reader, &attributes))
            return false;
    } else if (element->type != StackElement::None) {
        bool topLevel = element->type == StackElement::Root
                        || element->type == StackElement::SuppressedWarning
                        || element->type == StackElement::Rejection
                        || element->type == StackElement::LoadTypesystem
                        || element->type == StackElement::InjectCode
                        || element->type == StackElement::ExtraIncludes
                        || element->type == StackElement::SystemInclude
                        || element->type == StackElement::ConversionRule
                        || element->type == StackElement::AddFunction
                        || element->type == StackElement::Template;

        if (!topLevel && m_current->type == StackElement::Root) {
            m_error = QStringLiteral("Tag requires parent: '%1'").arg(tagName);
            return false;
        }

        StackElement topElement = !m_current ? StackElement(nullptr) : *m_current;
        element->entry = topElement.entry;

        switch (element->type) {
        case StackElement::Root:
            element->entry = parseRootElement(reader, versionRange.since, &attributes);
            element->type = StackElement::Root;
            break;
        case StackElement::LoadTypesystem:
            if (!loadTypesystem(reader, &attributes))
                return false;
            break;
        case StackElement::RejectEnumValue:
            if (!parseRejectEnumValue(reader, &attributes))
                return false;
            break;
        case StackElement::ReplaceType:
            if (!parseReplaceArgumentType(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::ConversionRule:
            if (!TypeSystemParser::parseCustomConversion(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::NativeToTarget:
            if (!parseNativeToTarget(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::TargetToNative: {
            if (topElement.type != StackElement::ConversionRule) {
                m_error = QLatin1String("Target to Native conversions can only be specified for custom conversion rules.");
                return false;
            }
            const int replaceIndex = indexOfAttribute(attributes, replaceAttribute());
            const bool replace = replaceIndex == -1
                || convertBoolean(attributes.takeAt(replaceIndex).value(),
                                  replaceAttribute(), true);
            m_current->entry->customConversion()->setReplaceOriginalTargetToNativeConversions(replace);
        }
        break;
        case StackElement::AddConversion:
            if (!parseAddConversion(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::ModifyArgument:
            if (!parseModifyArgument(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::NoNullPointers:
            if (!parseNoNullPointer(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::DefineOwnership:
            if (!parseDefineOwnership(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::SuppressedWarning: {
            const int textIndex = indexOfAttribute(attributes, textAttribute());
            if (textIndex == -1) {
                qCWarning(lcShiboken) << "Suppressed warning with no text specified";
            } else {
                const QString suppressedWarning =
                    attributes.takeAt(textIndex).value().toString();
                if (!m_database->addSuppressedWarning(suppressedWarning, &m_error))
                    return false;
            }
        }
            break;
        case StackElement::Rename:
             if (!parseRename(reader, topElement, &attributes))
                 return false;
             break;
        case StackElement::RemoveArgument:
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = QLatin1String("Removing argument requires argument modification as parent");
                return false;
            }

            m_contextStack.top()->functionMods.last().argument_mods().last().setRemoved(true);
            break;

        case StackElement::ModifyField:
            if (!parseModifyField(reader, &attributes))
                return false;
            break;
        case StackElement::DeclareFunction:
        case StackElement::AddFunction:
            if (!parseAddFunction(reader, topElement, element->type, &attributes))
                return false;
            break;
        case StackElement::Property:
            if (!parseProperty(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::ModifyFunction:
            if (!parseModifyFunction(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::ReplaceDefaultExpression:
            if (!parseReplaceDefaultExpression(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::RemoveDefaultExpression:
            m_contextStack.top()->functionMods.last().argument_mods().last().setRemovedDefaultExpression(true);
            break;
        case StackElement::CustomMetaConstructor:
        case StackElement::CustomMetaDestructor:
            element->value.customFunction =
                parseCustomMetaConstructor(reader, element->type, topElement, &attributes);
            break;
        case StackElement::ReferenceCount:
            if (!parseReferenceCount(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::ParentOwner:
            if (!parseParentOwner(reader, topElement, &attributes))
                return false;
            break;
        case StackElement::Array:
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = QLatin1String("array must be child of modify-argument");
                return false;
            }
            m_contextStack.top()->functionMods.last().argument_mods().last().setArray(true);
            break;
        case StackElement::InjectCode:
            if (!parseInjectCode(reader, topElement, element.get(), &attributes))
                return false;
            break;
        case StackElement::Include:
            if (!parseInclude(reader, topElement, element->entry, &attributes))
                return false;
            break;
        case StackElement::Rejection:
            if (!addRejection(m_database, &attributes, &m_error))
                return false;
            break;
        case StackElement::SystemInclude:
            if (!parseSystemInclude(reader, &attributes))
                return false;
            break;
        case StackElement::Template: {
            const int nameIndex = indexOfAttribute(attributes, nameAttribute());
            if (nameIndex == -1) {
                m_error = msgMissingAttribute(nameAttribute());
                return false;
            }
            element->value.templateEntry =
                new TemplateEntry(attributes.takeAt(nameIndex).value().toString());
        }
            break;
        case StackElement::TemplateInstanceEnum:
            element->value.templateInstance =
                parseTemplateInstanceEnum(reader, topElement, &attributes);
            if (!element->value.templateInstance)
                return false;
            break;
        case StackElement::Replace:
            if (!parseReplace(reader, topElement, element.get(), &attributes))
                return false;
            break;
        default:
            break; // nada
        }
    }

    if (!attributes.isEmpty()) {
        const QString message = msgUnusedAttributes(tagName, attributes);
        qCWarning(lcShiboken, "%s", qPrintable(msgReaderWarning(reader, message)));
    }

    m_current = element.release();
    return true;
}
