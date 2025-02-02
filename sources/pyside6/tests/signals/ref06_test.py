#!/usr/bin/env python
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

import gc
import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtCore import QObject, QTimeLine, Signal, Slot
from helper.usesqapplication import UsesQApplication


class ExtQObject(QObject):
    signalbetween = Signal('qreal')

    def __init__(self):
        super().__init__()
        self.counter = 0

    @Slot('qreal')
    def foo(self, value):
        self.counter += 1


class SignaltoSignalTest(UsesQApplication):

    def setUp(self):
        UsesQApplication.setUp(self)
        self.receiver = ExtQObject()
        self.timeline = QTimeLine(100)

    def tearDown(self):
        del self.timeline
        del self.receiver
        # PYSIDE-535: Need to collect garbage in PyPy to trigger deletion
        gc.collect()
        UsesQApplication.tearDown(self)

    def testSignaltoSignal(self):
        self.timeline.setUpdateInterval(10)

        self.timeline.finished.connect(self.app.quit)

        self.timeline.valueChanged.connect(self.receiver.signalbetween)
        self.receiver.signalbetween.connect(self.receiver.foo)

        self.timeline.start()

        self.app.exec()

        self.assertTrue(self.receiver.counter > 1)


if __name__ == '__main__':
    unittest.main()
