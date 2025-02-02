#!/usr/bin/env python
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

'''Test case for a class that holds a void pointer.'''

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()

from sample import ValueWithUnitUser, ValueWithUnitDoubleInch


class TypeSysTypeDefTest(unittest.TestCase):
    '''Test case type system typedefs.'''

    def test(self):
        inch_value = ValueWithUnitDoubleInch(10)
        mm_value = ValueWithUnitUser.doubleInchToMillimeter(inch_value)
        self.assertEqual(int(mm_value.value()), 2540)


if __name__ == '__main__':
    unittest.main()
