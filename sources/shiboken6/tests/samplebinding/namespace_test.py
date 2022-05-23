#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the test suite of Qt for Python.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

'''Test cases for std::map container conversions'''

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()

from sample import *
from shiboken_test_helper import objectFullname

from shiboken6 import Shiboken
_init_pyside_extension()   # trigger bootstrap

from shibokensupport.signature import get_signature

# For tests of invisible namespaces, see
# enumfromremovednamespace_test.py / removednamespaces.h


class TestVariablesUnderNamespace(unittest.TestCase):
    def testIt(self):
         self.assertEqual(SampleNamespace.variableInNamespace, 42)


class TestClassesUnderNamespace(unittest.TestCase):
    def testIt(self):
        if sys.pyside63_option_python_enum:
            c1 = SampleNamespace.SomeClass()
            e1 = SampleNamespace.SomeClass.ProtectedEnum(0)
            c2 = SampleNamespace.SomeClass.SomeInnerClass()
            e2 = SampleNamespace.SomeClass.SomeInnerClass.ProtectedEnum(0)
            c3 = SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough()
            e3 = SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough.NiceEnum(0)
        else:
            c1 = SampleNamespace.SomeClass()
            e1 = SampleNamespace.SomeClass.ProtectedEnum()
            c2 = SampleNamespace.SomeClass.SomeInnerClass()
            e2 = SampleNamespace.SomeClass.SomeInnerClass.ProtectedEnum()
            c3 = SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough()
            e3 = SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough.NiceEnum()

    def testFunctionAddedOnNamespace(self):
        res = SampleNamespace.ImInsideANamespace(2, 2)
        self.assertEqual(res, 4)

    def testTpNames(self):
        self.assertEqual(str(SampleNamespace.SomeClass),
            "<class 'sample.SampleNamespace.SomeClass'>")
        self.assertEqual(str(SampleNamespace.SomeClass.ProtectedEnum),
            "<enum 'ProtectedEnum'>" if sys.pyside63_option_python_enum else
            "<class 'sample.SampleNamespace.SomeClass.ProtectedEnum'>")
        self.assertEqual(str(SampleNamespace.SomeClass.SomeInnerClass.ProtectedEnum),
            "<enum 'ProtectedEnum'>" if sys.pyside63_option_python_enum else
            "<class 'sample.SampleNamespace.SomeClass.SomeInnerClass.ProtectedEnum'>")
        self.assertEqual(str(SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough),
            "<class 'sample.SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough'>")
        self.assertEqual(str(SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough.NiceEnum),
            "<enum 'NiceEnum'>" if sys.pyside63_option_python_enum else
            "<class 'sample.SampleNamespace.SomeClass.SomeInnerClass.OkThisIsRecursiveEnough.NiceEnum'>")

        # Test if enum inside of class is correct represented
        self.assertEqual(objectFullname(get_signature(SampleNamespace.enumInEnumOut).parameters['in_'].annotation),
            "sample.SampleNamespace.InValue")
        self.assertEqual(objectFullname(get_signature(SampleNamespace.enumAsInt).parameters['value'].annotation),
            "sample.SampleNamespace.SomeClass.PublicScopedEnum")


if __name__ == '__main__':
    unittest.main()
