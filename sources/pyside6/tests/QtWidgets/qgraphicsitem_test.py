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

''' Test cases related to QGraphicsItem and subclasses'''

import gc
import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtGui import QPolygonF, QColor, QBrush
from PySide6.QtWidgets import QGraphicsScene

from helper.usesqapplication import UsesQApplication


class QColorOnSetBrush(UsesQApplication):
    '''Test case for passing a QColor directly to setBrush'''

    def setUp(self):
        # Acquire resources
        super(QColorOnSetBrush, self).setUp()

        self.scene = QGraphicsScene()
        poly = QPolygonF()
        self.item = self.scene.addPolygon(poly)
        self.color = QColor('black')

    def tearDown(self):
        # Release resources
        del self.color
        del self.item
        del self.scene
        # PYSIDE-535: Need to collect garbage in PyPy to trigger deletion
        gc.collect()
        super(QColorOnSetBrush, self).tearDown()

    def testQColor(self):
        # QGraphicsAbstractShapeItem.setBrush(QColor)
        self.item.setBrush(self.color)
        self.assertEqual(QBrush(self.color), self.item.brush())


if __name__ == '__main__':
    unittest.main()
