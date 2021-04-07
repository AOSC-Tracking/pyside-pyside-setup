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

from distutils.version import LooseVersion

import os
import platform
import re
import sys
from textwrap import dedent
import time
from .config import config
from .utils import get_python_dict
from .options import DistUtilsCommandMixin, OPTION
from .versions import PYSIDE, PYSIDE_MODULE, SHIBOKEN
from .wheel_utils import (get_package_version, get_qt_version,
                          get_package_timestamp, macos_plat_name,
                          macos_pyside_min_deployment_target)


setup_script_dir = os.getcwd()
build_scripts_dir = os.path.join(setup_script_dir, 'build_scripts')
setup_py_path = os.path.join(setup_script_dir, "setup.py")

start_time = int(time.time())


def elapsed():
    return int(time.time()) - start_time


def get_setuptools_extension_modules():
    # Setting py_limited_api on the extension is the "correct" thing
    # to do, but it doesn't actually do anything, because we
    # override build_ext. So this is just foolproofing for the
    # future.
    extension_args = ('QtCore', [])
    extension_kwargs = {}
    if OPTION["LIMITED_API"] == 'yes':
        extension_kwargs['py_limited_api'] = True
    extension_modules = [Extension(*extension_args, **extension_kwargs)]
    return extension_modules


def _get_make(platform_arch, build_type):
    """Helper for retrieving the make command and CMake generator name"""
    makespec = OPTION["MAKESPEC"]
    if makespec == "make":
        return ("make", "Unix Makefiles")
    if makespec == "msvc":
        nmake_path = find_executable("nmake")
        if nmake_path is None or not os.path.exists(nmake_path):
            log.info("nmake not found. Trying to initialize the MSVC env...")
            init_msvc_env(platform_arch, build_type)
            nmake_path = find_executable("nmake")
            if not nmake_path or not os.path.exists(nmake_path):
                raise DistutilsSetupError('"nmake" could not be found.')
        if not OPTION["NO_JOM"]:
            jom_path = find_executable("jom")
            if jom_path:
                log.info(f"jom was found in {jom_path}")
                return (jom_path, "NMake Makefiles JOM")
        log.info(f"nmake was found in {nmake_path}")
        if OPTION["JOBS"]:
            msg = "Option --jobs can only be used with 'jom' on Windows."
            raise DistutilsSetupError(msg)
        return (nmake_path, "NMake Makefiles")
    if makespec == "mingw":
        return ("mingw32-make", "mingw32-make")
    if makespec == "ninja":
        return ("ninja", "Ninja")
    raise DistutilsSetupError(f'Invalid option --make-spec "{makespec}".')


def get_make(platform_arch, build_type):
    """Retrieve the make command and CMake generator name"""
    (make_path, make_generator) = _get_make(platform_arch, build_type)
    if not os.path.isabs(make_path):
        found_path = find_executable(make_path)
        if not found_path or not os.path.exists(found_path):
            m = f"You need the program '{make_path}' on your system path to compile {PYSIDE_MODULE}."
            raise DistutilsSetupError(m)
        make_path = found_path
    return (make_path, make_generator)


def _get_py_library_win(build_type, py_version, py_prefix, py_libdir,
                        py_include_dir):
    """Helper for finding the Python library on Windows"""
    if py_include_dir is None or not os.path.exists(py_include_dir):
        py_include_dir = os.path.join(py_prefix, "include")
    if py_libdir is None or not os.path.exists(py_libdir):
        # For virtual environments on Windows, the py_prefix will contain a
        # path pointing to it, instead of the system Python installation path.
        # Since INCLUDEPY contains a path to the system location, we use the
        # same base directory to define the py_libdir variable.
        py_libdir = os.path.join(os.path.dirname(py_include_dir), "libs")
        if not os.path.isdir(py_libdir):
            raise DistutilsSetupError("Failed to locate the 'libs' directory")
    dbg_postfix = "_d" if build_type == "Debug" else ""
    if OPTION["MAKESPEC"] == "mingw":
        static_lib_name = f"libpython{py_version.replace('.', '')}{dbg_postfix}.a"
        return os.path.join(py_libdir, static_lib_name)
    v = py_version.replace(".", "")
    python_lib_name = f"python{v}{dbg_postfix}.lib"
    return os.path.join(py_libdir, python_lib_name)


def _get_py_library_unix(build_type, py_version, py_prefix, py_libdir,
                         py_include_dir):
    """Helper for finding the Python library on UNIX"""
    if py_libdir is None or not os.path.exists(py_libdir):
        py_libdir = os.path.join(py_prefix, "lib")
    if py_include_dir is None or not os.path.exists(py_include_dir):
        dir = f"include/python{py_version}"
        py_include_dir = os.path.join(py_prefix, dir)
    dbg_postfix = "_d" if build_type == "Debug" else ""
    lib_exts = ['.so']
    if sys.platform == 'darwin':
        lib_exts.append('.dylib')
    lib_suff = getattr(sys, 'abiflags', None)
    lib_exts.append('.so.1')
    # Suffix for OpenSuSE 13.01
    lib_exts.append('.so.1.0')
    # static library as last gasp
    lib_exts.append('.a')

    libs_tried = []
    for lib_ext in lib_exts:
        lib_name = f"libpython{py_version}{lib_suff}{lib_ext}"
        py_library = os.path.join(py_libdir, lib_name)
        if os.path.exists(py_library):
            return py_library
        libs_tried.append(py_library)

    # Try to find shared libraries which have a multi arch
    # suffix.
    py_multiarch = get_config_var("MULTIARCH")
    if py_multiarch:
        try_py_libdir = os.path.join(py_libdir, py_multiarch)
        libs_tried = []
        for lib_ext in lib_exts:
            lib_name = f"libpython{py_version}{lib_suff}{lib_ext}"
            py_library = os.path.join(try_py_libdir, lib_name)
            if os.path.exists(py_library):
                return py_library
            libs_tried.append(py_library)

    raise DistutilsSetupError(f"Failed to locate the Python library with {', '.join(libs_tried)}")


def get_py_library(build_type, py_version, py_prefix, py_libdir, py_include_dir):
    """Find the Python library"""
    if sys.platform == "win32":
        py_library = _get_py_library_win(build_type, py_version, py_prefix,
                                         py_libdir, py_include_dir)
    else:
        py_library = _get_py_library_unix(build_type, py_version, py_prefix,
                                          py_libdir, py_include_dir)
    if py_library.endswith('.a'):
        # Python was compiled as a static library
        log.error(f"Failed to locate a dynamic Python library, using {py_library}")
    return py_library


import setuptools  # Import setuptools before distutils
from setuptools import Extension
from setuptools.command.install import install as _install
from setuptools.command.install_lib import install_lib as _install_lib
from setuptools.command.bdist_egg import bdist_egg as _bdist_egg
from setuptools.command.develop import develop as _develop
from setuptools.command.build_py import build_py as _build_py

import distutils.log as log
from distutils.errors import DistutilsSetupError
from distutils.sysconfig import get_config_var
from distutils.sysconfig import get_python_lib
from distutils.spawn import find_executable
from distutils.command.build import build as _build
from distutils.command.build_ext import build_ext as _build_ext
from distutils.cmd import Command

from .qtinfo import QtInfo
from .utils import rmtree, detect_clang, copyfile, copydir, run_process_output, run_process
from .utils import update_env_path, init_msvc_env, filter_match
from .utils import macos_fix_rpaths_for_library
from .utils import linux_fix_rpaths_for_library
from .platforms.unix import prepare_packages_posix
from .platforms.windows_desktop import prepare_packages_win32
from .wheel_override import wheel_module_exists, get_bdist_wheel_override


def check_allowed_python_version():
    """
    Make sure that setup.py is run with an allowed python version.
    """

    import re
    pattern = r'Programming Language :: Python :: (\d+)\.(\d+)'
    supported = []

    for line in config.python_version_classifiers:
        found = re.search(pattern, line)
        if found:
            major = int(found.group(1))
            minor = int(found.group(2))
            supported.append((major, minor))
    this_py = sys.version_info[:2]
    if this_py not in supported:
        log.error(f"Unsupported python version detected. Supported versions: {supported}")
        sys.exit(1)


qt_src_dir = ''


def is_debug_python():
    return getattr(sys, "gettotalrefcount", None) is not None


# Return a prefix suitable for the _install/_build directory
def prefix():
    virtual_env_name = os.environ.get('VIRTUAL_ENV', None)
    if virtual_env_name is not None:
        name = os.path.basename(virtual_env_name)
    else:
        name = "pyside"
    name += str(sys.version_info[0])
    if OPTION["DEBUG"]:
        name += "d"
    if is_debug_python():
        name += "p"
    if OPTION["LIMITED_API"] == "yes":
        name += "a"
    return name


# Initialize, pull and checkout submodules
def prepare_sub_modules():
    v = get_package_version()
    print(f"Initializing submodules for {PYSIDE_MODULE} version: {v}")
    submodules_dir = os.path.join(setup_script_dir, "sources")

    # Create list of [name, desired branch, absolute path, desired
    # branch] and determine whether all submodules are present
    need_init_sub_modules = False

    for m in submodules:
        module_name = m[0]
        module_dir = m[1] if len(m) > 1 else ''
        module_dir = os.path.join(submodules_dir, module_dir, module_name)
        # Check for non-empty directory (repository checked out)
        if not os.listdir(module_dir):
            need_init_sub_modules = True
            break

    if need_init_sub_modules:
        git_update_cmd = ["git", "submodule", "update", "--init"]
        if run_process(git_update_cmd) != 0:
            m = "Failed to initialize the git submodules: update --init failed"
            raise DistutilsSetupError(m)
        git_pull_cmd = ["git", "submodule", "foreach", "git", "fetch", "--all"]
        if run_process(git_pull_cmd) != 0:
            m = "Failed to initialize the git submodules: git fetch --all failed"
            raise DistutilsSetupError(m)
    else:
        print("All submodules present.")

    git_update_cmd = ["git", "submodule", "update"]
    if run_process(git_update_cmd) != 0:
        m = "Failed to checkout the correct git submodules SHA1s."
        raise DistutilsSetupError(m)


def prepare_build():
    # Clean up temp build folder.
    for n in ["build"]:
        d = os.path.join(setup_script_dir, n)
        if os.path.isdir(d):
            log.info(f"Removing {d}")
            try:
                rmtree(d)
            except Exception as e:
                log.warn(f'***** problem removing "{d}"')
                log.warn(f'ignored error: {e}')

    # locate Qt sources for the documentation
    if OPTION["QT_SRC"] is None:
        install_prefix = QtInfo().prefix_dir
        if install_prefix:
            global qt_src_dir
            # In-source, developer build
            if install_prefix.endswith("qtbase"):
                qt_src_dir = install_prefix
            else:  # SDK: Use 'Src' directory
                qt_src_dir = os.path.join(os.path.dirname(install_prefix), 'Src', 'qtbase')


class PysideInstall(_install, DistUtilsCommandMixin):

    user_options = _install.user_options + DistUtilsCommandMixin.mixin_user_options

    def __init__(self, *args, **kwargs):
        _install.__init__(self, *args, **kwargs)
        DistUtilsCommandMixin.__init__(self)

    def initialize_options(self):
        _install.initialize_options(self)

        if sys.platform == 'darwin':
            # Because we change the plat_name to include a correct
            # deployment target on macOS distutils thinks we are
            # cross-compiling, and throws an exception when trying to
            # execute setup.py install. The check looks like this
            # if self.warn_dir and build_plat != get_platform():
            #   raise DistutilsPlatformError("Can't install when "
            #                                  "cross-compiling")
            # Obviously get_platform will return the old deployment
            # target. The fix is to disable the warn_dir flag, which
            # was created for bdist_* derived classes to override, for
            # similar cases.
            self.warn_dir = False

    def finalize_options(self):
        DistUtilsCommandMixin.mixin_finalize_options(self)
        _install.finalize_options(self)

    def run(self):
        _install.run(self)
        log.info(f"--- Install completed ({elapsed()}s)")


class PysideDevelop(_develop):

    def __init__(self, *args, **kwargs):
        _develop.__init__(self, *args, **kwargs)

    def run(self):
        self.run_command("build")
        _develop.run(self)


class PysideBdistEgg(_bdist_egg):

    def __init__(self, *args, **kwargs):
        _bdist_egg.__init__(self, *args, **kwargs)

    def run(self):
        self.run_command("build")
        _bdist_egg.run(self)


class PysideBuildExt(_build_ext):

    def __init__(self, *args, **kwargs):
        _build_ext.__init__(self, *args, **kwargs)

    def run(self):
        pass


class PysideBuildPy(_build_py):

    def __init__(self, *args, **kwargs):
        _build_py.__init__(self, *args, **kwargs)


# _install_lib is reimplemented to preserve
# symlinks when distutils / setuptools copy files to various
# directories from the setup tools build dir to the install dir.
class PysideInstallLib(_install_lib):

    def __init__(self, *args, **kwargs):
        _install_lib.__init__(self, *args, **kwargs)

    def install(self):
        """
        Installs files from build/xxx directory into final
        site-packages/PySide6 directory.
        """

        if os.path.isdir(self.build_dir):
            # Using our own copydir makes sure to preserve symlinks.
            outfiles = copydir(os.path.abspath(self.build_dir), os.path.abspath(self.install_dir))
        else:
            self.warn(f"'{self.build_dir}' does not exist -- no Python modules to install")
            return
        return outfiles


class PysideBuild(_build, DistUtilsCommandMixin):

    user_options = _build.user_options + DistUtilsCommandMixin.mixin_user_options

    def __init__(self, *args, **kwargs):
        _build.__init__(self, *args, **kwargs)
        DistUtilsCommandMixin.__init__(self)

    def finalize_options(self):
        os_name_backup = os.name
        DistUtilsCommandMixin.mixin_finalize_options(self)
        if sys.platform == 'darwin':
            self.plat_name = macos_plat_name()
            # This is a hack to circumvent the dubious check in
            # distutils.commands.build -> finalize_options, which only
            # allows setting the plat_name for windows NT.
            # That is not the case for the wheel module though (which
            # does allow setting plat_name), so we circumvent by faking
            # the os name when finalizing the options, and then
            # restoring the original os name.
            os.name = "nt"

        _build.finalize_options(self)

        if sys.platform == 'darwin':
            os.name = os_name_backup

    def initialize_options(self):
        _build.initialize_options(self)
        self.make_path = None
        self.make_generator = None
        self.script_dir = None
        self.sources_dir = None
        self.build_dir = None
        self.install_dir = None
        self.py_executable = None
        self.py_include_dir = None
        self.py_library = None
        self.py_version = None
        self.py_arch = None
        self.build_type = "Release"
        self.qtinfo = None
        self.build_tests = False

    def run(self):
        prepare_build()
        platform_arch = platform.architecture()[0]
        log.info(f"Python architecture is {platform_arch}")
        self.py_arch = platform_arch[:-3]

        build_type = "Debug" if OPTION["DEBUG"] else "Release"
        if OPTION["RELWITHDEBINFO"]:
            build_type = 'RelWithDebInfo'

        # Check env
        make_path = None
        make_generator = None
        if not OPTION["ONLYPACKAGE"]:
            (make_path, make_generator) = get_make(platform_arch, build_type)

        # Prepare parameters
        py_executable = sys.executable
        py_version = f"{sys.version_info[0]}.{sys.version_info[1]}"
        py_include_dir = get_config_var("INCLUDEPY")
        py_libdir = get_config_var("LIBDIR")
        py_prefix = get_config_var("prefix")
        if not py_prefix or not os.path.exists(py_prefix):
            py_prefix = sys.prefix
        self.py_prefix = py_prefix
        if sys.platform == "win32":
            py_scripts_dir = os.path.join(py_prefix, "Scripts")
        else:
            py_scripts_dir = os.path.join(py_prefix, "bin")
        self.py_scripts_dir = py_scripts_dir

        self.qtinfo = QtInfo()
        qt_dir = os.path.dirname(OPTION["QMAKE"])
        qt_version = get_qt_version()

        # Update the PATH environment variable
        additional_paths = [self.py_scripts_dir, qt_dir]

        # Add Clang to path for Windows.
        # Revisit once Clang is bundled with Qt.
        if (sys.platform == "win32"
                and LooseVersion(self.qtinfo.version) >= LooseVersion("5.7.0")):
            clang_dir = detect_clang()
            if clang_dir[0]:
                clangBinDir = os.path.join(clang_dir[0], 'bin')
                if clangBinDir not in os.environ.get('PATH'):
                    log.info(f"Adding {clangBinDir} as detected by {clang_dir[1]} to PATH")
                    additional_paths.append(clangBinDir)
            else:
                raise DistutilsSetupError("Failed to detect Clang when checking "
                                          "LLVM_INSTALL_DIR, CLANG_INSTALL_DIR, llvm-config")

        update_env_path(additional_paths)

        # Used for test blacklists and registry test.
        self.build_classifiers = (f"py{py_version}-qt{qt_version}-{platform.architecture()[0]}-"
                                  f"{build_type.lower()}")

        if OPTION["SHORTER_PATHS"]:
            build_name = f"p{py_version}"
        else:
            build_name = self.build_classifiers

        script_dir = setup_script_dir
        sources_dir = os.path.join(script_dir, "sources")
        build_dir = os.path.join(script_dir, f"{prefix()}_build", f"{build_name}")
        install_dir = os.path.join(script_dir, f"{prefix()}_install", f"{build_name}")

        self.make_path = make_path
        self.make_generator = make_generator
        self.script_dir = script_dir
        self.st_build_dir = os.path.join(self.script_dir, self.build_lib)
        self.sources_dir = sources_dir
        self.build_dir = build_dir
        self.install_dir = install_dir
        self.py_executable = py_executable
        self.py_include_dir = py_include_dir
        self.py_library = get_py_library(build_type, py_version, py_prefix,
                                         py_libdir, py_include_dir)
        self.py_version = py_version
        self.build_type = build_type
        self.site_packages_dir = get_python_lib(1, 0, prefix=install_dir)
        self.build_tests = OPTION["BUILDTESTS"]

        # Save the shiboken build dir path for clang deployment
        # purposes.
        self.shiboken_build_dir = os.path.join(self.build_dir, SHIBOKEN)

        self.log_pre_build_info()

        # Prepare folders
        if not os.path.exists(self.sources_dir):
            log.info(f"Creating sources folder {self.sources_dir}...")
            os.makedirs(self.sources_dir)
        if not os.path.exists(self.build_dir):
            log.info(f"Creating build folder {self.build_dir}...")
            os.makedirs(self.build_dir)
        if not os.path.exists(self.install_dir):
            log.info(f"Creating install folder {self.install_dir}...")
            os.makedirs(self.install_dir)

        if (not OPTION["ONLYPACKAGE"]
                and not config.is_internal_shiboken_generator_build_and_part_of_top_level_all()):
            # Build extensions
            for ext in config.get_buildable_extensions():
                self.build_extension(ext)

            if OPTION["BUILDTESTS"]:
                # we record the latest successful build and note the
                # build directory for supporting the tests.
                timestamp = time.strftime('%Y-%m-%d_%H%M%S')
                build_history = os.path.join(setup_script_dir, 'build_history')
                unique_dir = os.path.join(build_history, timestamp)
                os.makedirs(unique_dir)
                fpath = os.path.join(unique_dir, 'build_dir.txt')
                with open(fpath, 'w') as f:
                    print(build_dir, file=f)
                    print(self.build_classifiers, file=f)
                log.info(f"Created {build_history}")

        if not OPTION["SKIP_PACKAGING"]:
            # Build patchelf if needed
            self.build_patchelf()

            # Prepare packages
            self.prepare_packages()

            # Build packages
            _build.run(self)
        else:
            log.info("Skipped preparing and building packages.")
        log.info(f"--- Build completed ({elapsed()}s)")

    def log_pre_build_info(self):
        if config.is_internal_shiboken_generator_build_and_part_of_top_level_all():
            return

        setuptools_install_prefix = get_python_lib(1)
        if OPTION["FINAL_INSTALL_PREFIX"]:
            setuptools_install_prefix = OPTION["FINAL_INSTALL_PREFIX"]
        log.info("=" * 30)
        log.info(f"Package version: {get_package_version()}")
        log.info(f"Build type:  {self.build_type}")
        log.info(f"Build tests: {self.build_tests}")
        log.info("-" * 3)
        log.info(f"Make path:      {self.make_path}")
        log.info(f"Make generator: {self.make_generator}")
        log.info(f"Make jobs:      {OPTION['JOBS']}")
        log.info("-" * 3)
        log.info(f"setup.py directory:      {self.script_dir}")
        log.info(f"Build scripts directory: {build_scripts_dir}")
        log.info(f"Sources directory:       {self.sources_dir}")
        log.info(dedent(f"""
        Building {config.package_name()} will create and touch directories
          in the following order:
            make build directory (py*_build/*/*) ->
            make install directory (py*_install/*/*) ->
            setuptools build directory (build/*/*) ->
            setuptools install directory
              (usually path-installed-python/lib/python*/site-packages/*)
         """))
        log.info(f"make build directory:   {self.build_dir}")
        log.info(f"make install directory: {self.install_dir}")
        log.info(f"setuptools build directory:   {self.st_build_dir}")
        log.info(f"setuptools install directory: {setuptools_install_prefix}")
        log.info(dedent(f"""
        make-installed site-packages directory: {self.site_packages_dir}
         (only relevant for copying files from 'make install directory'
                                          to   'setuptools build directory'
         """))
        log.info("-" * 3)
        log.info(f"Python executable: {self.py_executable}")
        log.info(f"Python includes:   {self.py_include_dir}")
        log.info(f"Python library:    {self.py_library}")
        log.info(f"Python prefix:     {self.py_prefix}")
        log.info(f"Python scripts:    {self.py_scripts_dir}")
        log.info("-" * 3)
        log.info(f"Qt qmake:   {self.qtinfo.qmake_command}")
        log.info(f"Qt version: {self.qtinfo.version}")
        log.info(f"Qt bins:    {self.qtinfo.bins_dir}")
        log.info(f"Qt docs:    {self.qtinfo.docs_dir}")
        log.info(f"Qt plugins: {self.qtinfo.plugins_dir}")
        log.info("-" * 3)
        if sys.platform == 'win32':
            log.info(f"OpenSSL dll directory: {OPTION['OPENSSL']}")
        if sys.platform == 'darwin':
            pyside_macos_deployment_target = (macos_pyside_min_deployment_target())
            log.info(f"MACOSX_DEPLOYMENT_TARGET set to: {pyside_macos_deployment_target}")
        log.info("=" * 30)

    def build_patchelf(self):
        if not sys.platform.startswith('linux'):
            return
        self._patchelf_path = find_executable('patchelf')
        if self._patchelf_path:
            if not os.path.isabs(self._patchelf_path):
                self._patchelf_path = os.path.join(os.getcwd(), self._patchelf_path)
            log.info(f"Using {self._patchelf_path} ...")
            return
        log.info("Building patchelf...")
        module_src_dir = os.path.join(self.sources_dir, "patchelf")
        build_cmd = ["g++", f"{module_src_dir}/patchelf.cc", "-o", "patchelf"]
        if run_process(build_cmd) != 0:
            raise DistutilsSetupError("Error building patchelf")
        self._patchelf_path = os.path.join(self.script_dir, "patchelf")

    def build_extension(self, extension):
        # calculate the subrepos folder name

        log.info(f"Building module {extension}...")

        # Prepare folders
        os.chdir(self.build_dir)
        module_build_dir = os.path.join(self.build_dir, extension)
        skipflag_file = f"{module_build_dir} -skip"
        if os.path.exists(skipflag_file):
            log.info(f"Skipping {extension} because {skipflag_file} exists")
            return

        module_build_exists = os.path.exists(module_build_dir)
        if module_build_exists:
            if not OPTION["REUSE_BUILD"]:
                log.info(f"Deleting module build folder {module_build_dir}...")
                try:
                    rmtree(module_build_dir)
                except Exception as e:
                    log.error(f'***** problem removing "{module_build_dir}"')
                    log.error(f'ignored error: {e}')
            else:
                log.info(f"Reusing module build folder {module_build_dir}...")
        if not os.path.exists(module_build_dir):
            log.info(f"Creating module build folder {module_build_dir}...")
            os.makedirs(module_build_dir)
        os.chdir(module_build_dir)

        module_src_dir = os.path.join(self.sources_dir, extension)

        # Build module
        cmake_cmd = [OPTION["CMAKE"]]
        if OPTION["QUIET"]:
            # Pass a special custom option, to allow printing a lot less information when doing
            # a quiet build.
            cmake_cmd.append('-DQUIET_BUILD=1')
            if self.make_generator == "Unix Makefiles":
                # Hide progress messages for each built source file.
                # Doesn't seem to work if set within the cmake files themselves.
                cmake_cmd.append('-DCMAKE_RULE_MESSAGES=0')

        cmake_cmd += [
            "-G", self.make_generator,
            f"-DBUILD_TESTS={self.build_tests}",
            f"-DQt5Help_DIR={self.qtinfo.docs_dir}",
            f"-DCMAKE_BUILD_TYPE={self.build_type}",
            f"-DCMAKE_INSTALL_PREFIX={self.install_dir}",
            module_src_dir
        ]
        cmake_cmd.append(f"-DPYTHON_EXECUTABLE={self.py_executable}")
        cmake_cmd.append(f"-DPYTHON_INCLUDE_DIR={self.py_include_dir}")
        cmake_cmd.append(f"-DPYTHON_LIBRARY={self.py_library}")

        # If a custom shiboken cmake config directory path was provided, pass it to CMake.
        if OPTION["SHIBOKEN_CONFIG_DIR"] and config.is_internal_pyside_build():
            config_dir = OPTION["SHIBOKEN_CONFIG_DIR"]
            if os.path.exists(config_dir):
                log.info(f"Using custom provided {SHIBOKEN} installation: {config_dir}")
                cmake_cmd.append(f"-DShiboken6_DIR={config_dir}")
            else:

                log.info(f"Custom provided {SHIBOKEN} installation not found. Path given: {config_dir}")

        if OPTION["MODULE_SUBSET"]:
            module_sub_set = ''
            for m in OPTION["MODULE_SUBSET"].split(','):
                if m.startswith('Qt'):
                    m = m[2:]
                if module_sub_set:
                    module_sub_set += ';'
                module_sub_set += m
            cmake_cmd.append(f"-DMODULES={module_sub_set}")
        if OPTION["SKIP_MODULES"]:
            skip_modules = ''
            for m in OPTION["SKIP_MODULES"].split(','):
                if m.startswith('Qt'):
                    m = m[2:]
                if skip_modules:
                    skip_modules += ';'
                skip_modules += m
            cmake_cmd.append(f"-DSKIP_MODULES={skip_modules}")
        # Add source location for generating documentation
        cmake_src_dir = OPTION["QT_SRC"] if OPTION["QT_SRC"] else qt_src_dir
        cmake_cmd.append(f"-DQT_SRC_DIR={cmake_src_dir}")
        if OPTION['SKIP_DOCS']:
            cmake_cmd.append("-DSKIP_DOCS=yes")
        log.info(f"Qt Source dir: {cmake_src_dir}")

        if self.build_type.lower() == 'debug':
            cmake_cmd.append(f"-DPYTHON_DEBUG_LIBRARY={self.py_library}")

        if OPTION["LIMITED_API"] == "yes":
            cmake_cmd.append("-DFORCE_LIMITED_API=yes")
        elif OPTION["LIMITED_API"] == "no":
            cmake_cmd.append("-DFORCE_LIMITED_API=no")
        elif not OPTION["LIMITED_API"]:
            pass
        else:
            raise DistutilsSetupError("option limited-api must be 'yes' or 'no' "
                                      "(default yes if applicable, i.e. python version >= 3.6)")

        if OPTION["VERBOSE_BUILD"]:
            cmake_cmd.append("-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON")

        if OPTION["SANITIZE_ADDRESS"]:
            # Some simple sanity checking. Only use at your own risk.
            if (sys.platform.startswith('linux')
                    or sys.platform.startswith('darwin')):
                cmake_cmd.append("-DSANITIZE_ADDRESS=ON")
            else:
                raise DistutilsSetupError("Address sanitizer can only be used on Linux and macOS.")

        if extension.lower() == PYSIDE:
            pyside_qt_conf_prefix = ''
            if OPTION["QT_CONF_PREFIX"]:
                pyside_qt_conf_prefix = OPTION["QT_CONF_PREFIX"]
            else:
                if OPTION["STANDALONE"]:
                    pyside_qt_conf_prefix = '"Qt"'
                if sys.platform == 'win32':
                    pyside_qt_conf_prefix = '"."'
            cmake_cmd.append(f"-DPYSIDE_QT_CONF_PREFIX={pyside_qt_conf_prefix}")

        # Pass package version to CMake, so this string can be
        # embedded into _config.py file.
        package_version = get_package_version()
        cmake_cmd.append(f"-DPACKAGE_SETUP_PY_PACKAGE_VERSION={package_version}")

        # In case if this is a snapshot build, also pass the
        # timestamp as a separate value, because it is the only
        # version component that is actually generated by setup.py.
        timestamp = ''
        if OPTION["SNAPSHOT_BUILD"]:
            timestamp = get_package_timestamp()
        cmake_cmd.append(f"-DPACKAGE_SETUP_PY_PACKAGE_TIMESTAMP={timestamp}")

        if extension.lower() in [SHIBOKEN]:
            cmake_cmd.append("-DCMAKE_INSTALL_RPATH_USE_LINK_PATH=yes")
            cmake_cmd.append("-DUSE_PYTHON_VERSION=3.6")

        if sys.platform == 'darwin':
            if OPTION["MACOS_ARCH"]:
                # also tell cmake which architecture to use
                cmake_cmd.append(f"-DCMAKE_OSX_ARCHITECTURES:STRING={OPTION['MACOS_ARCH']}")

            if OPTION["MACOS_USE_LIBCPP"]:
                # Explicitly link the libc++ standard library (useful
                # for macOS deployment targets lower than 10.9).
                # This is not on by default, because most libraries and
                # executables on macOS <= 10.8 are linked to libstdc++,
                # and mixing standard libraries can lead to crashes.
                # On macOS >= 10.9 with a similar minimum deployment
                # target, libc++ is linked in implicitly, thus the
                # option is a no-op in those cases.
                cmake_cmd.append("-DOSX_USE_LIBCPP=ON")

            if OPTION["MACOS_SYSROOT"]:
                cmake_cmd.append(f"-DCMAKE_OSX_SYSROOT={OPTION['MACOS_SYSROOT']}")
            else:
                latest_sdk_path = run_process_output(['xcrun', '--sdk', 'macosx',
                                                      '--show-sdk-path'])
                if latest_sdk_path:
                    latest_sdk_path = latest_sdk_path[0]
                    cmake_cmd.append(f"-DCMAKE_OSX_SYSROOT={latest_sdk_path}")

            # Set macOS minimum deployment target (version).
            # This is required so that calling
            #   run_process -> distutils.spawn()
            # does not set its own minimum deployment target
            # environment variable which is based on the python
            # interpreter sysconfig value.
            # Doing so could break the detected clang include paths
            # for example.
            deployment_target = macos_pyside_min_deployment_target()
            cmake_cmd.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={deployment_target}")
            os.environ['MACOSX_DEPLOYMENT_TARGET'] = deployment_target
        elif sys.platform == 'win32':
            # Prevent cmake from auto-detecting clang if it is in path.
            cmake_cmd.append("-DCMAKE_C_COMPILER=cl.exe")
            cmake_cmd.append("-DCMAKE_CXX_COMPILER=cl.exe")

        if not OPTION["SKIP_DOCS"]:
            # Build the whole documentation (rst + API) by default
            cmake_cmd.append("-DFULLDOCSBUILD=1")

            if OPTION["DOC_BUILD_ONLINE"]:
                log.info("Output format will be HTML")
                cmake_cmd.append("-DDOC_OUTPUT_FORMAT=html")
            else:
                log.info("Output format will be qthelp")
                cmake_cmd.append("-DDOC_OUTPUT_FORMAT=qthelp")
        else:
            cmake_cmd.append("-DSKIP_DOCS=1")

        if not OPTION["SKIP_CMAKE"]:
            log.info(f"Configuring module {extension} ({module_src_dir})...")
            if run_process(cmake_cmd) != 0:
                raise DistutilsSetupError(f"Error configuring {extension}")
        else:
            log.info(f"Reusing old configuration for module {extension} ({module_src_dir})...")

        log.info(f"-- Compiling module {extension}...")
        cmd_make = [self.make_path]
        if OPTION["JOBS"]:
            cmd_make.append(OPTION["JOBS"])
        if run_process(cmd_make) != 0:
            raise DistutilsSetupError(f"Error compiling {extension}")

        if not OPTION["SKIP_DOCS"]:
            if extension.lower() == SHIBOKEN:
                try:
                    # Check if sphinx is installed to proceed.
                    import sphinx

                    log.info("Generating Shiboken documentation")
                    if run_process([self.make_path, "doc"]) != 0:
                        raise DistutilsSetupError("Error generating documentation "
                                                  f"for {extension}")
                except ImportError:
                    log.info("Sphinx not found, skipping documentation build")
        else:
            log.info("Skipped documentation generation")
            cmake_cmd.append("-DSKIP_DOCS=1")

        if not OPTION["SKIP_MAKE_INSTALL"]:
            log.info(f"Installing module {extension}...")
            # Need to wait a second, so installed file timestamps are
            # older than build file timestamps.
            # See https://gitlab.kitware.com/cmake/cmake/issues/16155
            # for issue details.
            if sys.platform == 'darwin':
                log.info("Waiting 1 second, to ensure installation is successful...")
                time.sleep(1)
            # ninja: error: unknown target 'install/fast'
            target = 'install/fast' if self.make_generator != 'Ninja' else 'install'
            if run_process([self.make_path, target]) != 0:
                raise DistutilsSetupError(f"Error pseudo installing {extension}")
        else:
            log.info(f"Skipped installing module {extension}")

        os.chdir(self.script_dir)

    def prepare_packages(self):
        """
        This will copy all relevant files from the various locations in the "cmake install dir",
        to the setup tools build dir (which is read from self.build_lib provided by distutils).

        After that setuptools.command.build_py is smart enough to copy everything
        from the build dir to the install dir (the virtualenv site-packages for example).
        """
        try:
            log.info("\nPreparing setup tools build directory.\n")
            vars = {
                "site_packages_dir": self.site_packages_dir,
                "sources_dir": self.sources_dir,
                "install_dir": self.install_dir,
                "build_dir": self.build_dir,
                "script_dir": self.script_dir,
                "st_build_dir": self.st_build_dir,
                "cmake_package_name": config.package_name(),
                "st_package_name": config.package_name(),
                "ssl_libs_dir": OPTION["OPENSSL"],
                "py_version": self.py_version,
                "qt_version": self.qtinfo.version,
                "qt_bin_dir": self.qtinfo.bins_dir,
                "qt_doc_dir": self.qtinfo.docs_dir,
                "qt_lib_dir": self.qtinfo.libs_dir,
                "qt_lib_execs_dir": self.qtinfo.lib_execs_dir,
                "qt_plugins_dir": self.qtinfo.plugins_dir,
                "qt_prefix_dir": self.qtinfo.prefix_dir,
                "qt_translations_dir": self.qtinfo.translations_dir,
                "qt_qml_dir": self.qtinfo.qml_dir,
                "target_arch": self.py_arch,
            }

            # Needed for correct file installation in generator build
            # case.
            if config.is_internal_shiboken_generator_build():
                vars['cmake_package_name'] = config.shiboken_module_option_name

            os.chdir(self.script_dir)

            if sys.platform == "win32":
                vars['dbg_postfix'] = OPTION["DEBUG"] and "_d" or ""
                return prepare_packages_win32(self, vars)
            else:
                return prepare_packages_posix(self, vars)
        except IOError as e:
            print('setup.py/prepare_packages: ', e)
            raise

    def qt_is_framework_build(self):
        if os.path.isdir(self.qtinfo.headers_dir + "/../lib/QtCore.framework"):
            return True
        return False

    def get_built_pyside_config(self, vars):
        # Get config that contains list of built modules, and
        # SOVERSIONs of the built libraries.
        st_build_dir = vars['st_build_dir']
        config_path = os.path.join(st_build_dir, config.package_name(), "_config.py")
        temp_config = get_python_dict(config_path)
        if 'built_modules' not in temp_config:
            temp_config['built_modules'] = []
        return temp_config

    def is_webengine_built(self, built_modules):
        return ('WebEngineWidgets' in built_modules
                or 'WebEngineCore' in built_modules
                or 'WebEngine' in built_modules)

    def prepare_standalone_clang(self, is_win=False):
        """
        Copies the libclang library to the shiboken6-generator
        package so that the shiboken executable works.
        """
        log.info('Finding path to the libclang shared library.')
        cmake_cmd = [
            OPTION["CMAKE"],
            "-L",         # Lists variables
            "-N",         # Just inspects the cache (faster)
            "--build",    # Specifies the build dir
            self.shiboken_build_dir
        ]
        out = run_process_output(cmake_cmd)
        lines = [s.strip() for s in out]
        pattern = re.compile(r"CLANG_LIBRARY:FILEPATH=(.+)$")

        clang_lib_path = None
        for line in lines:
            match = pattern.search(line)
            if match:
                clang_lib_path = match.group(1)
                break

        if not clang_lib_path:
            raise RuntimeError("Could not find the location of the libclang "
                               "library inside the CMake cache file.")

        if is_win:
            # clang_lib_path points to the static import library
            # (lib/libclang.lib), whereas we want to copy the shared
            # library (bin/libclang.dll).
            clang_lib_path = re.sub(r'lib/libclang.lib$',
                                    'bin/libclang.dll',
                                    clang_lib_path)
        else:
            # shiboken6 links against libclang.so.6 or a similarly
            # named library.
            # If the linked against library is a symlink, resolve
            # the symlink once (but not all the way to the real
            # file) on Linux and macOS,
            # so that we get the path to the "SO version" symlink
            # (the one used as the install name in the shared library
            # dependency section).
            # E.g. On Linux libclang.so -> libclang.so.6 ->
            # libclang.so.6.0.
            # "libclang.so.6" is the name we want for the copied file.
            if os.path.islink(clang_lib_path):
                link_target = os.readlink(clang_lib_path)
                if os.path.isabs(link_target):
                    clang_lib_path = link_target
                else:
                    # link_target is relative, transform to absolute.
                    clang_lib_path = os.path.join(os.path.dirname(clang_lib_path), link_target)
            clang_lib_path = os.path.abspath(clang_lib_path)

        # The destination will be the shiboken package folder.
        vars = {}
        vars['st_build_dir'] = self.st_build_dir
        vars['st_package_name'] = config.package_name()
        destination_dir = "{st_build_dir}/{st_package_name}".format(**vars)

        if os.path.exists(clang_lib_path):
            basename = os.path.basename(clang_lib_path)
            log.info(f"Copying libclang shared library {clang_lib_path} to the package "
                     f"folder as {basename}.")
            destination_path = os.path.join(destination_dir, basename)

            # Need to modify permissions in case file is not writable
            # (a reinstall would cause a permission denied error).
            copyfile(clang_lib_path,
                     destination_path,
                     force_copy_symlink=True,
                     make_writable_by_owner=True)
        else:
            raise RuntimeError("Error copying libclang library "
                               f"from {clang_lib_path} to {destination_dir}. ")

    def update_rpath(self, package_path, executables):
        if sys.platform.startswith('linux'):
            pyside_libs = [lib for lib in os.listdir(
                package_path) if filter_match(lib, ["*.so", "*.so.*"])]

            def rpath_cmd(srcpath):
                final_rpath = ''
                # Command line rpath option takes precedence over
                # automatically added one.
                if OPTION["RPATH_VALUES"]:
                    final_rpath = OPTION["RPATH_VALUES"]
                else:
                    # Add rpath values pointing to $ORIGIN and the
                    # installed qt lib directory.
                    final_rpath = self.qtinfo.libs_dir
                    if OPTION["STANDALONE"]:
                        final_rpath = "$ORIGIN/Qt/lib"
                override = OPTION["STANDALONE"]
                linux_fix_rpaths_for_library(self._patchelf_path, srcpath, final_rpath,
                                             override=override)

        elif sys.platform == 'darwin':
            pyside_libs = [lib for lib in os.listdir(
                package_path) if filter_match(lib, ["*.so", "*.dylib"])]

            def rpath_cmd(srcpath):
                final_rpath = ''
                # Command line rpath option takes precedence over
                # automatically added one.
                if OPTION["RPATH_VALUES"]:
                    final_rpath = OPTION["RPATH_VALUES"]
                else:
                    if OPTION["STANDALONE"]:
                        final_rpath = "@loader_path/Qt/lib"
                    else:
                        final_rpath = self.qtinfo.libs_dir
                macos_fix_rpaths_for_library(srcpath, final_rpath)

        else:
            raise RuntimeError(f"Not configured for platform {sys.platform}")

        pyside_libs.extend(executables)

        # Update rpath in PySide6 libs
        for srcname in pyside_libs:
            srcpath = os.path.join(package_path, srcname)
            if os.path.isdir(srcpath) or os.path.islink(srcpath):
                continue
            if not os.path.exists(srcpath):
                continue
            rpath_cmd(srcpath)
            log.info("Patched rpath to '$ORIGIN/' (Linux) or "
                     f"updated rpath (OS/X) in {srcpath}.")


class PysideRstDocs(Command, DistUtilsCommandMixin):
    description = "Build .rst documentation only"
    user_options = DistUtilsCommandMixin.mixin_user_options

    def initialize_options(self):
        DistUtilsCommandMixin.__init__(self)
        log.info("-- This build process will not include the API documentation."
                 "API documentation requires a full build of pyside/shiboken.")
        self.skip = False
        if config.is_internal_shiboken_generator_build():
            self.skip = True
        if not self.skip:
            self.name = config.package_name().lower()
            self.doc_dir = os.path.join(config.setup_script_dir, "sources")
            self.doc_dir = os.path.join(self.doc_dir, self.name)
            self.doc_dir = os.path.join(self.doc_dir, "doc")
            try:
                # Check if sphinx is installed to proceed.
                import sphinx
                if self.name == SHIBOKEN:
                    log.info("-- Generating Shiboken documentation")
                    log.info(f"-- Documentation directory: 'html/{PYSIDE}/{SHIBOKEN}/'")
                elif self.name == PYSIDE:
                    log.info("-- Generating PySide documentation")
                    log.info(f"-- Documentation directory: 'html/{PYSIDE}/'")
            except ImportError:
                raise DistutilsSetupError("Sphinx not found - aborting")
            self.html_dir = "html"

            # creating directories html/pyside6/shiboken6
            try:
                if not os.path.isdir(self.html_dir):
                    os.mkdir(self.html_dir)
                if self.name == SHIBOKEN:
                    out_pyside = os.path.join(self.html_dir, PYSIDE)
                    if not os.path.isdir(out_pyside):
                        os.mkdir(out_pyside)
                    out_shiboken = os.path.join(out_pyside, SHIBOKEN)
                    if not os.path.isdir(out_shiboken):
                        os.mkdir(out_shiboken)
                    self.out_dir = out_shiboken
                # We know that on the shiboken step, we already created the
                # 'pyside6' directory
                elif self.name == PYSIDE:
                    self.out_dir = os.path.join(self.html_dir, PYSIDE)
            except (PermissionError, FileExistsError):
                raise DistutilsSetupError(f"Error while creating directories for {self.doc_dir}")

    def run(self):
        if not self.skip:
            cmake_cmd = [OPTION["CMAKE"]]
            cmake_cmd += [
                "-S", self.doc_dir,
                "-B", self.out_dir,
                "-DDOC_OUTPUT_FORMAT=html",
                "-DFULLDOCSBUILD=0",
            ]
            if run_process(cmake_cmd) != 0:
                raise DistutilsSetupError(f"Error running CMake for {self.doc_dir}")

            if self.name == PYSIDE:
                self.sphinx_src = os.path.join(self.out_dir, "rst")
            elif self.name == SHIBOKEN:
                self.sphinx_src = self.out_dir

            sphinx_cmd = ["sphinx-build", "-b", "html", "-c", self.sphinx_src,
                          self.doc_dir, self.out_dir]
            if run_process(sphinx_cmd) != 0:
                raise DistutilsSetupError(f"Error running CMake for {self.doc_dir}")
        # Last message
        if not self.skip and self.name == PYSIDE:
            log.info(f"-- The documentation was built. Check html/{PYSIDE}/index.html")

    def finalize_options(self):
        DistUtilsCommandMixin.mixin_finalize_options(self)


cmd_class_dict = {
    'build': PysideBuild,
    'build_py': PysideBuildPy,
    'build_ext': PysideBuildExt,
    'bdist_egg': PysideBdistEgg,
    'develop': PysideDevelop,
    'install': PysideInstall,
    'install_lib': PysideInstallLib,
    'build_rst_docs': PysideRstDocs,
}
if wheel_module_exists:
    pyside_bdist_wheel = get_bdist_wheel_override()
    if pyside_bdist_wheel:
        cmd_class_dict['bdist_wheel'] = pyside_bdist_wheel
