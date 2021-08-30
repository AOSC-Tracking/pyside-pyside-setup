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

'''Simple test case for accessing the exposed C++ class fields.'''

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()

from sample import Derived, Point, ObjectType

class TestAccessingCppFields(unittest.TestCase):
    '''Simple test case for accessing the exposed C++ class fields.'''

    def testAccessingPrimitiveTypeField(self):
        '''Reads and writes a primitive type (in this case an 'int') field.'''
        d = Derived()
        self.assertEqual(type(d.primitiveField), int)

        # attribution
        old_value = d.primitiveField
        new_value = 2255
        d.primitiveField = new_value
        self.assertEqual(d.primitiveField, new_value)
        self.assertNotEqual(d.primitiveField, old_value)

        # attribution with a convertible type
        value = 1.2
        d.primitiveField = value
        self.assertEqual(d.primitiveField, int(value))

        # attribution with invalid type
        self.assertRaises(TypeError, lambda : setattr(d, 'primitiveField', None))

    def testAccessingRenamedFields(self):
        '''Reads and writes a renamed field.'''
        d = Derived()
        self.assertEqual(type(d.renamedField), int)
        old_value = d.renamedField
        new_value = 2255
        d.renamedField = new_value
        self.assertEqual(d.renamedField, new_value)
        self.assertNotEqual(d.renamedField, old_value)

    def testAccessingReadOnlyFields(self):
        '''Tests a read-only field.'''
        d = Derived()
        self.assertEqual(type(d.readOnlyField), int)
        old_value = d.readOnlyField
        try:
            d.readOnlyField = 25555
        except AttributeError:
            pass
        self.assertEqual(d.readOnlyField, old_value)

    def testAccessingUsersPrimitiveTypeField(self):
        '''Reads and writes an user's primitive type (in this case an 'Complex') field.'''
        d = Derived()
        self.assertEqual(type(d.userPrimitiveField), complex)

        # attribution
        old_value = d.userPrimitiveField
        new_value = complex(1.1, 2.2)
        d.userPrimitiveField = new_value
        self.assertEqual(d.userPrimitiveField, new_value)
        self.assertNotEqual(d.userPrimitiveField, old_value)

        # attribution with invalid type
        self.assertRaises(TypeError, lambda : setattr(d, 'userPrimitiveField', None))

    def testAccessingValueTypeField(self):
        '''Reads and writes a value type (in this case a 'Point') field.'''
        d = Derived()
        self.assertEqual(type(d.valueTypeField), Point)

        # attribution
        old_value = d.valueTypeField
        new_value = Point(-10, 537)
        d.valueTypeField = new_value
        self.assertEqual(d.valueTypeField, new_value)

        #object modify
        d.valueTypeField.setX(10)
        d.valueTypeField.setY(20)
        self.assertEqual(d.valueTypeField.x(), 10)
        self.assertEqual(d.valueTypeField.y(), 20)

        # attribution with invalid type
        self.assertRaises(TypeError, lambda : setattr(d, 'valueTypeField', 123))

    def testAccessingObjectTypeField(self):
        '''Reads and writes a object type (in this case an 'ObjectType') field.'''
        d = Derived()

        # attribution
        old_value = d.objectTypeField
        new_value = ObjectType()
        d.objectTypeField = new_value
        self.assertEqual(d.objectTypeField, new_value)
        self.assertNotEqual(d.objectTypeField, old_value)

        # attribution with a convertible type
        value = None
        d.objectTypeField = value
        self.assertEqual(d.objectTypeField, value)

        # attribution with invalid type
        self.assertRaises(TypeError, lambda : setattr(d, 'objectTypeField', 123))

    @unittest.skipUnless(hasattr(sys, "getrefcount"), f"{sys.implementation.name} has no refcount")
    def testRefCountingAccessingObjectTypeField(self):
        '''Accessing a object type field should respect the reference counting rules.'''
        d = Derived()

        # attributing object to instance's field should increase its reference count
        o1 = ObjectType()
        refcount1 = sys.getrefcount(o1)
        d.objectTypeField = o1
        self.assertEqual(d.objectTypeField, o1)
        self.assertEqual(sys.getrefcount(d.objectTypeField), refcount1 + 1)

        # attributing a new object to instance's field should decrease the previous
        # object's reference count
        o2 = ObjectType()
        refcount2 = sys.getrefcount(o2)
        d.objectTypeField = o2
        self.assertEqual(d.objectTypeField, o2)
        self.assertEqual(sys.getrefcount(o1), refcount1)
        self.assertEqual(sys.getrefcount(d.objectTypeField), refcount2 + 1)

    @unittest.skipUnless(hasattr(sys, "getrefcount"), f"{sys.implementation.name} has no refcount")
    def testRefCountingOfReferredObjectAfterDeletingReferrer(self):
        '''Deleting the object referring to other object should decrease the
        reference count of the referee.'''
        d = Derived()
        o = ObjectType()
        refcount = sys.getrefcount(o)
        d.objectTypeField = o
        self.assertEqual(sys.getrefcount(o), refcount + 1)
        del d
        self.assertEqual(sys.getrefcount(o), refcount)

    def testStaticField(self):
        self.assertEqual(Derived.staticPrimitiveField, 0)

    def testAccessingUnsignedIntBitField(self):
        d = Derived()

        # attribution
        old_value = d.bitField
        new_value = 1
        d.bitField= new_value
        self.assertEqual(d.bitField, new_value)
        self.assertNotEqual(d.bitField, old_value)

        # attribution with a convertible type
        value = 1.2
        d.bitField = value
        self.assertEqual(d.bitField, int(value))

        # attribution with invalid type
        self.assertRaises(TypeError, lambda : setattr(d, 'bitField', None))


if __name__ == '__main__':
    unittest.main()
