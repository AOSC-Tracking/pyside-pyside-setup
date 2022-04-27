#############################################################################
##
## Copyright (C) 2020 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of Qt for Python.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtCore import QCborArray, QObject

is_pypy = hasattr(sys, "pypy_version_info")
if not is_pypy:
    from PySide6.support import feature

from textwrap import dedent

"""
multiple_feature_test.py
------------------------

This tests the selectable features in PySide.

The first feature is `snake_case` instead of `camelCase`.
There is much more to come.
"""

MethodDescriptorType = type(str.split)


@unittest.skipIf(is_pypy, "__feature__ cannot yet be used with PyPy")
class FeaturesTest(unittest.TestCase):

    def testAllFeatureCombinations(self):
        """
        Test for all 256 possible combinations of `__feature__` imports.
        """

        def tst_bit0(flag, self, bits):
            if flag == 0:
                QCborArray.isEmpty
                QCborArray.__dict__["isEmpty"]
                with self.assertRaises(AttributeError):
                    QCborArray.is_empty
                with self.assertRaises(KeyError):
                    QCborArray.__dict__["is_empty"]
            else:
                QCborArray.is_empty
                QCborArray.__dict__["is_empty"]
                with self.assertRaises(AttributeError):
                    QCborArray.isEmpty
                with self.assertRaises(KeyError):
                    QCborArray.__dict__["isEmpty"]

        def tst_bit1(flag, self, bits):
            getter_name = "object_name" if bits & 1 else "objectName"
            setter_name = "set_object_name" if bits & 1 else "setObjectName"
            thing = getattr(QObject, getter_name)
            if flag:
                self.assertEqual(type(thing), property)
                with self.assertRaises(AttributeError):
                    getattr(QObject, setter_name)
            else:
                self.assertEqual(type(thing), MethodDescriptorType)
                getattr(QObject, setter_name)

        edict = {}
        for bit in range(2, 8):
            # We are cheating here, since the functions are in the globals.

            bit_pow = 1 << bit
            eval(compile(dedent(f"""

        def tst_bit{bit}(flag, self, bits):
            if flag == 0:
                with self.assertRaises(AttributeError):
                    QCborArray.fake_feature_{bit_pow:02x}
                with self.assertRaises(KeyError):
                    QCborArray.__dict__["fake_feature_{bit_pow:02x}"]
            else:
                QCborArray.fake_feature_{bit_pow:02x}
                QCborArray.__dict__["fake_feature_{bit_pow:02x}"]

                        """), "<string>", "exec"), globals(), edict)
        globals().update(edict)
        feature_list = feature._really_all_feature_names
        func_list = [tst_bit0, tst_bit1, tst_bit2, tst_bit3,
                     tst_bit4, tst_bit5, tst_bit6, tst_bit7]

        for idx in range(0x100):
            feature.reset()
            config = f"feature_{idx:02x}"
            print()
            print(f"--- Feature Test Config `{config}` ---")
            print("Imports:")
            for bit in range(8):
                if idx & 1 << bit:
                    cur_feature = feature_list[bit]
                    text = f"from __feature__ import {cur_feature}"
                    print(text)
                    eval(compile(text, "<string>", "exec"), globals(), edict)
            for bit in range(8):
                value = idx & 1 << bit
                func_list[bit](value, self=self, bits=idx)


if __name__ == '__main__':
    unittest.main()
