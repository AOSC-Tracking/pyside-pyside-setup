# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

'''Test cases for QtAsyncio'''

import asyncio
import unittest

import PySide6.QtAsyncio as QtAsyncio


class QAsyncioTestCaseCancelTaskGroup(unittest.TestCase):
    def setUp(self) -> None:
        super().setUp()
        # We only reach the end of the loop if the task is not cancelled.
        self.loop_end_reached = False

    async def raise_error(self):
        raise RuntimeError

    async def loop_short(self):
        self._loop_end_reached = False
        for _ in range(1000):
            await asyncio.sleep(1e-3)
        self._loop_end_reached = True

    async def loop_shorter(self):
        self._loop_end_reached = False
        for _ in range(1000):
            await asyncio.sleep(1e-4)
        self._loop_end_reached = True

    async def loop_the_shortest(self):
        self._loop_end_reached = False
        for _ in range(1000):
            await asyncio.to_thread(lambda: None)
        self._loop_end_reached = True

    async def main(self, coro):
        async with asyncio.TaskGroup() as tg:
            tg.create_task(coro())
            tg.create_task(self.raise_error())

    def test_cancel_taskgroup(self):
        coros = [self.loop_short, self.loop_shorter, self.loop_the_shortest]

        for coro in coros:
            try:
                QtAsyncio.run(self.main(coro), keep_running=False)
            except ExceptionGroup as e:
                self.assertEqual(len(e.exceptions), 1)
                self.assertIsInstance(e.exceptions[0], RuntimeError)
                self.assertFalse(self._loop_end_reached)


if __name__ == '__main__':
    unittest.main()