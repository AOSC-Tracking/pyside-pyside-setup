/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of Qt for Python.
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

#include "testaddfunction.h"
#include <QtTest/QTest>
#include "testutil.h"
#include <abstractmetafunction.h>
#include <abstractmetalang.h>
#include <modifications.h>
#include <modifications_p.h>
#include <typesystem.h>

void TestAddFunction::testParsingFuncNameAndConstness()
{
    // generic test...
    const char sig1[] = "func(type1, const type2, const type3* const)";
    QString errorMessage;
    auto f1 = AddedFunction::createAddedFunction(QLatin1String(sig1), QLatin1String("void"),
                                                 &errorMessage);
    QVERIFY2(!f1.isNull(), qPrintable(errorMessage));
    QCOMPARE(f1->name(), u"func");
    QCOMPARE(f1->arguments().size(), 3);
    TypeInfo retval = f1->returnType();
    QCOMPARE(retval.qualifiedName(), QStringList{QLatin1String("void")});
    QCOMPARE(retval.indirections(), 0);
    QCOMPARE(retval.isConstant(), false);
    QCOMPARE(retval.referenceType(), NoReference);

    // test with a ugly template as argument and other ugly stuff
    const char sig2[] = "    _fu__nc_       (  type1, const type2, const Abc<int& , C<char*> *   >  * *@my_name@, const type3* const    )   const ";
    auto f2 = AddedFunction::createAddedFunction(QLatin1String(sig2),
                                                 QLatin1String("const Abc<int& , C<char*> *   >  * *"),
                                                 &errorMessage);
    QVERIFY2(!f2.isNull(), qPrintable(errorMessage));
    QCOMPARE(f2->name(), u"_fu__nc_");
    const auto &args = f2->arguments();
    QCOMPARE(args.size(), 4);
    retval = f2->returnType();
    QCOMPARE(retval.qualifiedName(), QStringList{QLatin1String("Abc")});
    QCOMPARE(retval.instantiations().size(), 2);
    QCOMPARE(retval.toString(), u"const Abc<int&, C<char*>*>**");
    QCOMPARE(retval.indirections(), 2);
    QCOMPARE(retval.isConstant(), true);
    QCOMPARE(retval.referenceType(), NoReference);
    QVERIFY(args.at(0).name.isEmpty());
    QVERIFY(args.at(1).name.isEmpty());

    QCOMPARE(args.at(2).name, u"my_name");
    auto arg2Type = args.at(2).typeInfo;
    QCOMPARE(arg2Type.qualifiedName(), QStringList{QLatin1String("Abc")});
    QCOMPARE(arg2Type.instantiations().size(), 2);
    QCOMPARE(arg2Type.toString(), u"const Abc<int&, C<char*>*>**");
    QCOMPARE(arg2Type.indirections(), 2);
    QCOMPARE(arg2Type.isConstant(), true);
    QCOMPARE(arg2Type.referenceType(), NoReference);

    QVERIFY(args.at(3).name.isEmpty());

    // function with no args.
    const char sig3[] = "func()";
    auto f3 = AddedFunction::createAddedFunction(QLatin1String(sig3), QLatin1String("void"),
                                                 &errorMessage);
    QVERIFY2(!f3.isNull(), qPrintable(errorMessage));
    QCOMPARE(f3->name(), u"func");
    QCOMPARE(f3->arguments().size(), 0);

    // const call operator
    const char sig4[] = "operator()(int)const";
    auto f4 = AddedFunction::createAddedFunction(QLatin1String(sig4), QLatin1String("int"),
                                                 &errorMessage);
    QVERIFY2(!f4.isNull(), qPrintable(errorMessage));
    QCOMPARE(f4->name(), u"operator()");
    QCOMPARE(f4->arguments().size(), 1);
    QVERIFY(f4->isConstant());
}

void TestAddFunction::testAddFunction()
{
    const char cppCode[] = R"CPP(
struct B {};
struct A {
    void a(int);
};)CPP";
    const char xmlCode[] = R"XML(
<typesystem package='Foo'>
    <primitive-type name='int'/>
    <primitive-type name='float'/>
    <value-type name='B'/>
    <value-type name='A'>
        <add-function signature='b(int, float = 4.6, const B&amp;)' return-type='int' access='protected'/>
        <add-function signature='operator()(int)' return-type='int' access='public'/>
    </value-type>
</typesystem>)XML";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    TypeDatabase* typeDb = TypeDatabase::instance();
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    // default ctor, default copy ctor, func a() and the added functions
    QCOMPARE(classA->functions().size(), 5);

    auto addedFunc = classA->findFunction(QLatin1String("b"));
    QVERIFY(addedFunc);
    QCOMPARE(addedFunc->access(), Access::Protected);
    QCOMPARE(addedFunc->functionType(), AbstractMetaFunction::NormalFunction);
    QVERIFY(addedFunc->isUserAdded());
    QCOMPARE(addedFunc->ownerClass(), classA);
    QCOMPARE(addedFunc->implementingClass(), classA);
    QCOMPARE(addedFunc->declaringClass(), classA);
    QVERIFY(!addedFunc->isVirtual());
    QVERIFY(!addedFunc->isSignal());
    QVERIFY(!addedFunc->isSlot());
    QVERIFY(!addedFunc->isStatic());

    AbstractMetaType returnType = addedFunc->type();
    QCOMPARE(returnType.typeEntry(), typeDb->findPrimitiveType(QLatin1String("int")));
    const AbstractMetaArgumentList &args = addedFunc->arguments();
    QCOMPARE(args.size(), 3);
    QCOMPARE(args.at(0).type().typeEntry(), returnType.typeEntry());
    QCOMPARE(args.at(1).defaultValueExpression(), u"4.6");
    QCOMPARE(args.at(2).type().typeEntry(), typeDb->findType(QLatin1String("B")));

    auto addedCallOperator = classA->findFunction(QLatin1String("operator()"));
    QVERIFY(addedCallOperator);
}

void TestAddFunction::testAddFunctionConstructor()
{
    const char cppCode[] = "struct A { A() {} };\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <value-type name='A'>\n\
            <add-function signature='A(int)'/>\n\
        </value-type>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    QCOMPARE(classA->functions().size(), 3); // default and added ctors
    const auto addedFunc = classA->functions().constLast();
    QCOMPARE(addedFunc->access(), Access::Public);
    QCOMPARE(addedFunc->functionType(), AbstractMetaFunction::ConstructorFunction);
    QCOMPARE(addedFunc->arguments().size(), 1);
    QVERIFY(addedFunc->isUserAdded());
    QVERIFY(addedFunc->isVoid());
}

void TestAddFunction::testAddFunctionTagDefaultValues()
{
    const char cppCode[] = "struct A {};\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <value-type name='A'>\n\
            <add-function signature='func()'/>\n\
        </value-type>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    // default ctor, default copy ctor and the added function
    QCOMPARE(classA->functions().size(), 3);
    const auto addedFunc = classA->functions().constLast();
    QCOMPARE(addedFunc->access(), Access::Public);
    QCOMPARE(addedFunc->functionType(), AbstractMetaFunction::NormalFunction);
    QVERIFY(addedFunc->isUserAdded());
    QVERIFY(addedFunc->isVoid());
}

void TestAddFunction::testAddFunctionCodeSnippets()
{
    const char cppCode[] = "struct A {};\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <value-type name='A'>\n\
            <add-function signature='func()'>\n\
                <inject-code class='target' position='end'>Hi!, I am the code.</inject-code>\n\
            </add-function>\n\
        </value-type>\n\
    </typesystem>\n";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    const auto addedFunc = classA->functions().constLast();
    QVERIFY(addedFunc->hasInjectedCode());
}

void TestAddFunction::testAddFunctionWithoutParenteses()
{
    const char sig1[] = "func";
    QString errorMessage;
    auto f1 = AddedFunction::createAddedFunction(QLatin1String(sig1), QLatin1String("void"),
                                                 &errorMessage);
    QVERIFY2(!f1.isNull(), qPrintable(errorMessage));
    QCOMPARE(f1->name(), u"func");
    QCOMPARE(f1->arguments().size(), 0);
    QCOMPARE(f1->isConstant(), false);

    const char cppCode[] = "struct A {};\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <value-type name='A'>\n\
            <add-function signature='func'>\n\
                <inject-code class='target' position='end'>Hi!, I am the code.</inject-code>\n\
            </add-function>\n\
        </value-type>\n\
    </typesystem>\n";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    const auto addedFunc = classA->findFunction(QLatin1String("func"));
    QVERIFY(!addedFunc.isNull());
    QVERIFY(addedFunc->hasInjectedCode());
    const auto snips = addedFunc->injectedCodeSnips(TypeSystem::CodeSnipPositionAny,
                                                    TypeSystem::TargetLangCode);
    QCOMPARE(snips.size(), 1);
}

void TestAddFunction::testAddFunctionWithDefaultArgs()
{
    const char sig1[] = "func";
    QString errorMessage;
    auto f1 = AddedFunction::createAddedFunction(QLatin1String(sig1), QLatin1String("void"),
                                                 &errorMessage);
    QVERIFY2(!f1.isNull(), qPrintable(errorMessage));
    QCOMPARE(f1->name(), u"func");
    QCOMPARE(f1->arguments().size(), 0);
    QCOMPARE(f1->isConstant(), false);

    const char cppCode[] = "struct A { };\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <value-type name='A'>\n\
            <add-function signature='func(int, int)'>\n\
              <modify-argument index='2'>\n\
                <replace-default-expression with='2'/>\n\
              </modify-argument>\n\
            </add-function>\n\
        </value-type>\n\
    </typesystem>\n";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    const auto addedFunc = classA->findFunction(QLatin1String("func"));
    QVERIFY(!addedFunc.isNull());
    const AbstractMetaArgument &arg = addedFunc->arguments().at(1);
    QCOMPARE(arg.defaultValueExpression(), u"2");
}

void TestAddFunction::testAddFunctionAtModuleLevel()
{
    const char cppCode[] = "struct A { };\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <value-type name='A'/>\n\
        <add-function signature='func(int, int)'>\n\
            <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
        </add-function>\n\
    </typesystem>\n";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);

    TypeDatabase* typeDb = TypeDatabase::instance();

    AddedFunctionList addedFuncs = typeDb->findGlobalUserFunctions(QLatin1String("func"));

    QCOMPARE(addedFuncs.size(), 1);

    const FunctionModificationList mods = addedFuncs.constFirst()->modifications;

    QCOMPARE(mods.size(), 1);
    QVERIFY(mods.constFirst().isCodeInjection());
    CodeSnip snip = mods.constFirst().snips().constFirst();
    QCOMPARE(snip.code().trimmed(), u"custom_code();");
}

void TestAddFunction::testAddFunctionWithVarargs()
{
    const char sig1[] = "func(int,char,...)";
    QString errorMessage;
    auto f1 = AddedFunction::createAddedFunction(QLatin1String(sig1), QLatin1String("void"),
                                                 &errorMessage);
    QVERIFY2(!f1.isNull(), qPrintable(errorMessage));
    QCOMPARE(f1->name(), u"func");
    QCOMPARE(f1->arguments().size(), 3);
    QVERIFY(!f1->isConstant());

    const char cppCode[] = "struct A {};\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <primitive-type name='char'/>\n\
        <value-type name='A'>\n\
            <add-function signature='func(int,char,...)'/>\n\
        </value-type>\n\
    </typesystem>\n";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    const auto addedFunc = classA->findFunction(QLatin1String("func"));
    QVERIFY(!addedFunc.isNull());
    const AbstractMetaArgument &arg = addedFunc->arguments().constLast();
    QVERIFY(arg.type().isVarargs());
    QVERIFY(arg.type().typeEntry()->isVarargs());
}

void TestAddFunction::testAddStaticFunction()
{
    const char cppCode[] = "struct A { };\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <value-type name='A'>\n\
            <add-function signature='func(int, int)' static='yes'>\n\
                <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
            </add-function>\n\
        </value-type>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    const auto addedFunc = classA->findFunction(QLatin1String("func"));
    QVERIFY(!addedFunc.isNull());
    QVERIFY(addedFunc->isStatic());
}

void TestAddFunction::testAddGlobalFunction()
{
    const char cppCode[] = "struct A { };struct B {};\n";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <value-type name='A'/>\n\
        <add-function signature='globalFunc(int, int)' static='yes'>\n\
            <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
        </add-function>\n\
        <add-function signature='globalFunc2(int, int)' static='yes'>\n\
            <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
        </add-function>\n\
        <value-type name='B'/>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    const auto globalFuncs = builder->globalFunctions();
    QCOMPARE(globalFuncs.size(), 2);
    const AbstractMetaClass *classB = AbstractMetaClass::findClass(builder->classes(), u"B");
    QVERIFY(classB);
    QVERIFY(!classB->findFunction(QLatin1String("globalFunc")));
    QVERIFY(!classB->findFunction(QLatin1String("globalFunc2")));
    QVERIFY(!globalFuncs[0]->injectedCodeSnips().isEmpty());
    QVERIFY(!globalFuncs[1]->injectedCodeSnips().isEmpty());
}

void TestAddFunction::testAddFunctionWithApiVersion()
{
    const char cppCode[] = "";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <add-function signature='globalFunc(int, int)' static='yes' since='1.3'>\n\
            <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
        </add-function>\n\
        <add-function signature='globalFunc2(int, int)' static='yes' since='0.1'>\n\
            <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
        </add-function>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode,
                                                                true, QLatin1String("0.1")));
    QVERIFY(!builder.isNull());
    const auto globalFuncs = builder->globalFunctions();
    QCOMPARE(globalFuncs.size(), 1);
}

void TestAddFunction::testModifyAddedFunction()
{
    const char cppCode[] = "class Foo { };\n";
    const char xmlCode[] = R"(
<typesystem package='Package'>
    <primitive-type name='float'/>
    <primitive-type name='int'/>
    <value-type name='Foo'>
        <add-function signature='method(float, int)'>
            <inject-code class='target' position='beginning'>custom_code();</inject-code>
            <modify-argument index='2' rename='varName'>
                <replace-default-expression with='0'/>
            </modify-argument>
        </add-function>
    </value-type>
</typesystem>
)";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    AbstractMetaClass* foo = AbstractMetaClass::findClass(classes, u"Foo");
    const auto method = foo->findFunction(QLatin1String("method"));
    QVERIFY(!method.isNull());
    QCOMPARE(method->arguments().size(), 2);
    const AbstractMetaArgument &arg = method->arguments().at(1);
    QCOMPARE(arg.defaultValueExpression(), u"0");
    QCOMPARE(arg.name(), u"varName");
    QCOMPARE(method->argumentName(2), u"varName");
}

void TestAddFunction::testAddFunctionOnTypedef()
{
    const char cppCode[] = "template<class T> class Foo { }; typedef Foo<int> FooInt;\n";
    const char xmlCode[] = "\
    <typesystem package='Package'>\n\
        <value-type name='FooInt'>\n\
            <add-function signature='FooInt(PySequence)'>\n\
                <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
            </add-function>\n\
            <add-function signature='method()'>\n\
                <inject-code class='target' position='beginning'>custom_code();</inject-code>\n\
            </add-function>\n\
        </value-type>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    AbstractMetaClass* foo = AbstractMetaClass::findClass(classes, u"FooInt");
    QVERIFY(foo);
    QVERIFY(foo->hasNonPrivateConstructor());
    const auto &lst = foo->queryFunctions(FunctionQueryOption::AnyConstructor);
    for (const auto &f : lst)
        QVERIFY(f->signature().startsWith(f->name()));
    QCOMPARE(lst.size(), 2);
    const auto method = foo->findFunction(QLatin1String("method"));
    QVERIFY(!method.isNull());
}

void TestAddFunction::testAddFunctionWithTemplateArg()
{
    const char cppCode[] = "template<class T> class Foo { };\n";
    const char xmlCode[] = "\
    <typesystem package='Package'>\n\
        <primitive-type name='int'/>\n\
        <container-type name='Foo' type='list'/>\n\
        <add-function signature='func(Foo&lt;int>)'/>\n\
    </typesystem>\n";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode));
    QVERIFY(!builder.isNull());
    QCOMPARE(builder->globalFunctions().size(), 1);
    const auto func = builder->globalFunctions().constFirst();
    const AbstractMetaArgument &arg = func->arguments().constFirst();
    QCOMPARE(arg.type().instantiations().size(), 1);
}

// Test splitting of <add-function> parameter lists.

Q_DECLARE_METATYPE(AddedFunctionParser::Argument)

using Arguments = AddedFunctionParser::Arguments;

void TestAddFunction::testAddFunctionTypeParser_data()
{
    QTest::addColumn<QString>("parameterList");
    QTest::addColumn<Arguments>("expected");

    QTest::newRow("empty")
        << QString() << Arguments{};

    QTest::newRow("1-arg")
       << QString::fromLatin1("int @a@=42")
       << Arguments{{QLatin1String("int"), QLatin1String("a"), QLatin1String("42")}};

    QTest::newRow("2-args")
       << QString::fromLatin1("double @d@, int @a@=42")
       << Arguments{{QLatin1String("double"), QLatin1String("d"), {}},
                    {QLatin1String("int"), QLatin1String("a"), QLatin1String("42")}};

    QTest::newRow("template-var_args")
       << QString::fromLatin1("const QList<X,Y> &@list@ = QList<X,Y>{1,2}, int @b@=5, ...")
       << Arguments{{QLatin1String("const QList<X,Y> &"), QLatin1String("list"), QLatin1String("QList<X,Y>{1,2}")},
                    {QLatin1String("int"), QLatin1String("b"), QLatin1String("5")},
                    {QLatin1String("..."), {}, {}}};
}

void TestAddFunction::testAddFunctionTypeParser()
{

    QFETCH(QString, parameterList);
    QFETCH(Arguments, expected);

    const auto actual = AddedFunctionParser::splitParameters(parameterList);
    QCOMPARE(actual, expected);
}

QTEST_APPLESS_MAIN(TestAddFunction)
