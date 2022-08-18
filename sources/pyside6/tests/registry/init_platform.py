#############################################################################
##
## Copyright (C) 2022 The Qt Company Ltd.
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

"""
init_platform.py

Existence registry
==================

This is a registry for all existing function signatures.
One file is generated with all signatures of a platform and version.

The scope has been extended to generate all signatures from the
shiboken and pysidetest projects.
"""

import sys
import os
from contextlib import contextmanager
from textwrap import dedent
from util import get_refpath, get_script_dir


def qt_build():
    result = '<Unknown build of Qt>'
    try:
        from PySide6.QtCore import QLibraryInfo
        result = QLibraryInfo.build()
    except:
        pass
    return result


script_dir = get_script_dir()
history_dir = os.path.join(script_dir, 'build_history')

# Find out if we have the build dir, already. Then use it.
look_for = os.path.join("pyside6", "tests", "pysidetest")
have_build_dir = [x for x in sys.path if x.endswith(look_for)]
if have_build_dir:
    all_build_dir = os.path.normpath(os.path.join(have_build_dir[0], "..", "..", ".."))
elif os.path.exists(history_dir):
    # Using the last build to find the build dir.
    # Note: This is not reliable when building in parallel!
    last_build = max(x for x in os.listdir(history_dir) if x.startswith("20"))
    fpath = os.path.join(history_dir, last_build, "build_dir.txt")
    if os.path.exists(fpath):
        with open(fpath) as f:
            f_contents = f.read().strip()
            f_contents_split = f_contents.splitlines()
            try:
                all_build_dir = f_contents_split[0]
            except IndexError:
                print(f"Error: can't find the build dir in the given file '{fpath}'")
                sys.exit(1)
else:
    print(dedent("""
        Can't find the build dir in the history.
        Compile again and don't forget to specify "--build-tests".
        """))
    sys.exit(1)

if not os.path.exists(os.path.join(all_build_dir, look_for)):
    print(dedent("""
        PySide has not been built with tests enabled.
        Compile again and don't forget to specify "--build-tests".
        """))
    sys.exit(1)

pyside_build_dir = os.path.join(all_build_dir, "pyside6")
shiboken_build_dir = os.path.join(all_build_dir, "shiboken6")

# now we compute all paths:


def set_ospaths(build_dir):
    ps = os.pathsep
    ospath_var = "PATH" if sys.platform == "win32" else "LD_LIBRARY_PATH"
    old_val = os.environ.get(ospath_var, "")
    lib_path = [os.path.join(build_dir, "pyside6", "tests", "pysidetest"),]
    ospath = ps.join(lib_path + old_val.split(ps))
    os.environ[ospath_var] = ospath


set_ospaths(all_build_dir)

import PySide6

all_modules = list("PySide6." + _ for _ in PySide6.__all__)

# now we should be able to do all imports:
if not have_build_dir:
    sys.path.insert(0, os.path.join(pyside_build_dir, "tests", "pysidetest"))
import testbinding
all_modules.append("testbinding")

from shiboken6 import Shiboken
all_modules.append("shiboken6.Shiboken")

from shibokensupport.signature.lib.enum_sig import SimplifyingEnumerator

# Make sure not to get .pyc in Python2.
sourcepath = os.path.splitext(__file__)[0] + ".py"


class Formatter(object):
    """
    Formatter is formatting the signature listing of an enumerator.

    It is written as context managers in order to avoid many callbacks.
    The separation in formatter and enumerator is done to keep the
    unrelated tasks of enumeration and formatting apart.
    """
    def __init__(self, outfile):
        self.outfile = outfile
        self.last_level = 0

    def print(self, *args, **kw):
        print(*args, file=self.outfile, **kw) if self.outfile else None

    @contextmanager
    def module(self, mod_name):
        self.print(f"")
        self.print(f"# Module {mod_name}")
        self.print(f"sig_dict.update({{")
        yield
        self.print(f'    }}) if "{mod_name}" in sys.modules else None')

    @contextmanager
    def klass(self, class_name, class_str):
        self.print()
        self.print(f"# class {self.mod_name}.{class_name}:")
        yield

    @contextmanager
    def function(self, func_name, signature):
        if self.last_level > self.level:
            self.print()
        self.last_level = self.level
        class_name = self.class_name
        if class_name is None:
            key = viskey = f"{self.mod_name}.{func_name}"
        else:
            key = viskey = f"{self.mod_name}.{class_name}.{func_name}"
        if key.endswith("lY"):
            # Some classes like PySide6.QtGui.QContextMenuEvent have functions
            # globalX and the same with Y. The gerrit robot thinks that this
            # is a badly written "globally". Convince it by hiding this word.
            viskey = viskey[:-1] + '""Y'
        self.print(f'    "{viskey}": {signature},')
        yield key


def enum_all():
    fmt = Formatter(None)
    enu = SimplifyingEnumerator(fmt)
    ret = enu.result_type()
    for mod_name in all_modules:
        ret.update(enu.module(mod_name))
    return ret


LICENSE_TEXT = """
#############################################################################
##
## Copyright (C) 2022 The Qt Company Ltd.
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
"""


def generate_all():
    refPath = get_refpath()
    module = os.path.basename(os.path.splitext(refPath)[0])
    with open(refPath, "w") as outfile, open(sourcepath) as f:
        fmt = Formatter(outfile)
        enu = SimplifyingEnumerator(fmt)
        lines = f.readlines()
        license_line = next((lno for lno, line in enumerate(lines)
                             if "$QT_END_LICENSE$" in line))
        fmt.print("#recreate       # uncomment this to enforce generation")
        fmt.print(LICENSE_TEXT)
        version = sys.version.replace('\n', ' ')
        build = qt_build()
        fmt.print(dedent(f'''\
            """
            This file contains the simplified signatures for all functions in PySide
            for module '{module}' using
            Python {version}
            {build}

            There are no default values, no variable names and no self
            parameter. Only types are present after simplification. The
            functions 'next' resp. '__next__' are removed to make the output
            identical for Python 2 and 3. '__div__' is also removed,
            since it exists in Python 2, only.
            """
            '''))
        fmt.print("import sys")
        fmt.print("")
        fmt.print("sig_dict = {}")
        for mod_name in all_modules:
            enu.module(mod_name)
        fmt.print("# eof")


def __main__():
    print(f"+++ generating {get_refpath()}. You should probably check this file in.")
    generate_all()


if __name__ == "__main__":
    __main__()
