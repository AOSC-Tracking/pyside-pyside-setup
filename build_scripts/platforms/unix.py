#############################################################################
##
## Copyright (C) 2018 The Qt Company Ltd.
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

import fnmatch
import os
import sys

from ..config import config
from ..options import OPTION
from ..utils import copydir, copyfile, makefile
from ..versions import PYSIDE, SHIBOKEN
from .linux import prepare_standalone_package_linux
from .macos import prepare_standalone_package_macos


def _macos_copy_gui_executable(name, vars=None):
    """macOS helper: Copy a GUI executable from the .app folder and return the
       files"""
    app_name = name[:1].upper() + name[1:] + '.app'
    return copydir(f"{{install_dir}}/bin/{app_name}",
                   f"{{st_build_dir}}/{{st_package_name}}/{app_name}",
                   filter=None, recursive=True,
                   force=False, vars=vars)


def _unix_copy_gui_executable(name, vars=None):
    """UNIX helper: Copy a GUI executable and return the files"""
    return copydir("{install_dir}/bin/",
                   "{st_build_dir}/{st_package_name}/",
                   filter=[name],
                   force=False, vars=vars)


def _copy_gui_executable(name, vars=None):
    """Copy a GUI executable and return the files"""
    if sys.platform == 'darwin':
        return _macos_copy_gui_executable(name, vars)
    return _unix_copy_gui_executable(name, vars)


def prepare_packages_posix(self, vars):
    executables = []
    libexec_executables = []

    # <install>/lib/site-packages/{st_package_name}/* ->
    # <setup>/{st_package_name}
    # This copies the module .so/.dylib files and various .py files
    # (__init__, config, git version, etc.)
    copydir(
        "{site_packages_dir}/{st_package_name}",
        "{st_build_dir}/{st_package_name}",
        vars=vars)

    generated_config = self.get_built_pyside_config(vars)

    def adjusted_lib_name(name, version):
        postfix = ''
        if sys.platform.startswith('linux'):
            postfix = '.so.' + version
        elif sys.platform == 'darwin':
            postfix = '.' + version + '.dylib'
        return name + postfix

    if config.is_internal_shiboken_module_build():
        # <build>/shiboken6/doc/html/* ->
        #   <setup>/{st_package_name}/docs/shiboken6
        copydir(
            f"{{build_dir}}/{SHIBOKEN}/doc/html",
            f"{{st_build_dir}}/{{st_package_name}}/docs/{SHIBOKEN}",
            force=False, vars=vars)

        # <install>/lib/lib* -> {st_package_name}/
        copydir(
            "{install_dir}/lib/",
            "{st_build_dir}/{st_package_name}",
            filter=[
                adjusted_lib_name("libshiboken*",
                                  generated_config['shiboken_library_soversion']),
            ],
            recursive=False, vars=vars, force_copy_symlinks=True)

    if config.is_internal_shiboken_generator_build():
        # <install>/bin/* -> {st_package_name}/
        executables.extend(copydir(
            "{install_dir}/bin/",
            "{st_build_dir}/{st_package_name}",
            filter=[SHIBOKEN],
            recursive=False, vars=vars))

        # Used to create scripts directory.
        makefile(
            "{st_build_dir}/{st_package_name}/scripts/shiboken_tool.py",
            vars=vars)

        # For setting up setuptools entry points.
        copyfile(
            "{install_dir}/bin/shiboken_tool.py",
            "{st_build_dir}/{st_package_name}/scripts/shiboken_tool.py",
            force=False, vars=vars)

    if config.is_internal_shiboken_generator_build() or config.is_internal_pyside_build():
        # <install>/include/* -> <setup>/{st_package_name}/include
        copydir(
            "{install_dir}/include/{cmake_package_name}",
            "{st_build_dir}/{st_package_name}/include",
            vars=vars)

    if config.is_internal_pyside_build():
        makefile(
            "{st_build_dir}/{st_package_name}/scripts/__init__.py",
            vars=vars)

        # For setting up setuptools entry points
        for script in ("pyside_tool.py", "metaobjectdump.py", "project.py"):
            src = f"{{install_dir}}/bin/{script}"
            target = f"{{st_build_dir}}/{{st_package_name}}/scripts/{script}"
            copyfile(src, target, force=False, vars=vars)

        # <install>/bin/* -> {st_package_name}/
        executables.extend(copydir(
            "{install_dir}/bin/",
            "{st_build_dir}/{st_package_name}",
            filter=[f"{PYSIDE}-lupdate"],
            recursive=False, vars=vars))

        lib_exec_filters = []
        if not OPTION['NO_QT_TOOLS']:
            lib_exec_filters.extend(['uic', 'rcc', 'qmltyperegistrar'])
            executables.extend(copydir(
                "{install_dir}/bin/",
                "{st_build_dir}/{st_package_name}",
                filter=["lrelease", "lupdate", "qmllint"],
                recursive=False, vars=vars))
            # Copying assistant/designer
            executables.extend(_copy_gui_executable('assistant', vars=vars))
            executables.extend(_copy_gui_executable('designer', vars=vars))
            executables.extend(_copy_gui_executable('linguist', vars=vars))

        # <qt>/lib/metatypes/* -> <setup>/{st_package_name}/Qt/lib/metatypes
        destination_lib_dir = "{st_build_dir}/{st_package_name}/Qt/lib"
        copydir("{qt_lib_dir}/metatypes", f"{destination_lib_dir}/metatypes",
                filter=["*.json"],
                recursive=False, vars=vars, force_copy_symlinks=True)

        # Copy libexec
        built_modules = self.get_built_pyside_config(vars)['built_modules']
        if self.is_webengine_built(built_modules):
            lib_exec_filters.append('QtWebEngineProcess')
        if lib_exec_filters:
            libexec_executables.extend(copydir("{qt_lib_execs_dir}",
                                               "{st_build_dir}/{st_package_name}/Qt/libexec",
                                               filter=lib_exec_filters,
                                               recursive=False,
                                               vars=vars))

        # <install>/lib/lib* -> {st_package_name}/
        copydir(
            "{install_dir}/lib/",
            "{st_build_dir}/{st_package_name}",
            filter=[
                adjusted_lib_name("libpyside*",
                                  generated_config['pyside_library_soversion']),
            ],
            recursive=False, vars=vars, force_copy_symlinks=True)

        # <install>/share/{st_package_name}/typesystems/* ->
        #   <setup>/{st_package_name}/typesystems
        copydir(
            "{install_dir}/share/{st_package_name}/typesystems",
            "{st_build_dir}/{st_package_name}/typesystems",
            vars=vars)

        # <install>/share/{st_package_name}/glue/* ->
        #   <setup>/{st_package_name}/glue
        copydir(
            "{install_dir}/share/{st_package_name}/glue",
            "{st_build_dir}/{st_package_name}/glue",
            vars=vars)

        # <source>/pyside6/{st_package_name}/support/* ->
        #   <setup>/{st_package_name}/support/*
        copydir(
            f"{{build_dir}}/{PYSIDE}/{{st_package_name}}/support",
            "{st_build_dir}/{st_package_name}/support",
            vars=vars)

        # <source>/pyside6/{st_package_name}/*.pyi ->
        #   <setup>/{st_package_name}/*.pyi
        copydir(
            f"{{build_dir}}/{PYSIDE}/{{st_package_name}}",
            "{st_build_dir}/{st_package_name}",
            filter=["*.pyi", "py.typed"],
            vars=vars)

        if not OPTION["NOEXAMPLES"]:
            def pycache_dir_filter(dir_name, parent_full_path, dir_full_path):
                if fnmatch.fnmatch(dir_name, "__pycache__"):
                    return False
                return True
            # examples/* -> <setup>/{st_package_name}/examples
            copydir(os.path.join(self.script_dir, "examples"),
                    "{st_build_dir}/{st_package_name}/examples",
                    force=False, vars=vars, dir_filter_function=pycache_dir_filter)

    # Copy Qt libs to package
    if OPTION["STANDALONE"]:
        if config.is_internal_pyside_build() or config.is_internal_shiboken_generator_build():
            vars['built_modules'] = generated_config['built_modules']
            if sys.platform == 'darwin':
                prepare_standalone_package_macos(self, vars)
            else:
                prepare_standalone_package_linux(self, vars)

        if config.is_internal_shiboken_generator_build():
            # Copy over clang before rpath patching.
            self.prepare_standalone_clang(is_win=False)

    # Update rpath to $ORIGIN
    if sys.platform.startswith('linux') or sys.platform.startswith('darwin'):
        rpath_path = "{st_build_dir}/{st_package_name}".format(**vars)
        self.update_rpath(rpath_path, executables)
        self.update_rpath(rpath_path, self.package_libraries(rpath_path))
        if libexec_executables:
            self.update_rpath(rpath_path, libexec_executables, libexec=True)
