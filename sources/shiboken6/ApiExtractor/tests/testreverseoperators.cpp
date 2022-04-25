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

#include "testreverseoperators.h"
#include <QtTest/QTest>
#include "testutil.h"
#include <abstractmetafunction.h>
#include <abstractmetalang.h>
#include <typesystem.h>

void TestReverseOperators::testReverseSum()
{
    const char cppCode[] = "struct A {\n\
            A& operator+(int);\n\
        };\n\
        A& operator+(int, const A&);";
    const char xmlCode[] = "\n\
    <typesystem package=\"Foo\">\n\
        <primitive-type name='int' />\n\
        <value-type name='A' />\n\
    </typesystem>";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode, false));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    AbstractMetaClass* classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    QCOMPARE(classA->functions().size(), 4);

    AbstractMetaFunctionCPtr reverseOp;
    AbstractMetaFunctionCPtr normalOp;
    for (const auto &func : classA->functions()) {
        if (func->name() == u"operator+") {
            if (func->isReverseOperator())
                reverseOp = func;
            else
                normalOp = func;
        }
    }

    QVERIFY(!normalOp.isNull());
    QVERIFY(!normalOp->isReverseOperator());
    QCOMPARE(normalOp->arguments().size(), 1);
    QVERIFY(!reverseOp.isNull());
    QVERIFY(reverseOp->isReverseOperator());
    QCOMPARE(reverseOp->arguments().size(), 1);
}

void TestReverseOperators::testReverseSumWithAmbiguity()
{
    const char cppCode[] = "\n\
    struct A { A operator+(int); };\n\
    A operator+(int, const A&);\n\
    struct B {};\n\
    B operator+(const A&, const B&);\n\
    B operator+(const B&, const A&);\n\
    ";
    const char xmlCode[] = "\n\
    <typesystem package=\"Foo\">\n\
        <primitive-type name='int' />\n\
        <value-type name='A' />\n\
        <value-type name='B' />\n\
    </typesystem>";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode, false));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, u"A");
    QVERIFY(classA);
    QCOMPARE(classA->functions().size(), 4);

    const AbstractMetaClass *classB = AbstractMetaClass::findClass(classes, u"B");
    QVERIFY(classB);
    QCOMPARE(classB->functions().size(), 4);

    AbstractMetaFunctionCPtr reverseOp;
    AbstractMetaFunctionCPtr normalOp;
    for (const auto &func : classB->functions()) {
        if (func->name() == u"operator+") {
            if (func->isReverseOperator())
                reverseOp = func;
            else
                normalOp = func;
        }
    }
    QVERIFY(!normalOp.isNull());
    QVERIFY(!normalOp->isReverseOperator());
    QCOMPARE(normalOp->arguments().size(), 1);
    QCOMPARE(normalOp->minimalSignature(), u"operator+(B,A)");
    QVERIFY(!reverseOp.isNull());
    QVERIFY(reverseOp->isReverseOperator());
    QCOMPARE(reverseOp->arguments().size(), 1);
    QCOMPARE(reverseOp->minimalSignature(), u"operator+(A,B)");
}



QTEST_APPLESS_MAIN(TestReverseOperators)

