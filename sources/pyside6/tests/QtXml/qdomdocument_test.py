#!/usr/bin/python

#############################################################################
##
## Copyright (C) 2021 The Qt Company Ltd.
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

import gc
import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtCore import QByteArray
from PySide6.QtXml import QDomDocument, QDomElement


class QDomDocumentTest(unittest.TestCase):

    def setUp(self):
        self.dom = QDomDocument()

        self.goodXmlData = QByteArray(bytes('''
        <typesystem package="PySide6.QtXml">
            <value-type name="QDomDocument"/>
            <value-type name="QDomElement"/>
        </typesystem>
        ''', "UTF-8"))

        self.badXmlData = QByteArray(bytes('''
        <typesystem package="PySide6.QtXml">
            <value-type name="QDomDocument">
        </typesystem>
        ''', "UTF-8"))

    def tearDown(self):
        del self.dom
        del self.goodXmlData
        del self.badXmlData
        # PYSIDE-535: Need to collect garbage in PyPy to trigger deletion
        gc.collect()

    def testQDomDocumentSetContentWithBadXmlData(self):
        '''Sets invalid xml as the QDomDocument contents.'''
        ok, errorStr, errorLine, errorColumn = self.dom.setContent(self.badXmlData, True)
        self.assertFalse(ok)
        self.assertEqual(errorStr, 'Opening and ending tag mismatch.')
        self.assertEqual(errorLine, 4)
        self.assertEqual(errorColumn, 21)

    def testQDomDocumentSetContentWithGoodXmlData(self):
        '''Sets valid xml as the QDomDocument contents.'''
        ok, errorStr, errorLine, errorColumn = self.dom.setContent(self.goodXmlData, True)
        self.assertTrue(ok)
        self.assertEqual(errorStr, '')
        self.assertEqual(errorLine, 0)
        self.assertEqual(errorColumn, 0)

    def testQDomDocumentData(self):
        '''Checks the QDomDocument elements for the valid xml contents.'''

        def checkAttribute(element, attribute, value):
            self.assertTrue(isinstance(root, QDomElement))
            self.assertFalse(element.isNull())
            self.assertTrue(element.hasAttribute(attribute))
            self.assertEqual(element.attribute(attribute), value)

        ok, errorStr, errorLine, errorColumn = self.dom.setContent(self.goodXmlData, True)
        root = self.dom.documentElement()
        self.assertEqual(root.tagName(), 'typesystem')
        checkAttribute(root, 'package', 'PySide6.QtXml')

        child = root.firstChildElement('value-type')
        checkAttribute(child, 'name', 'QDomDocument')

        child = child.nextSiblingElement('value-type')
        checkAttribute(child, 'name', 'QDomElement')


if __name__ == '__main__':
    unittest.main()

