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

import sys
import os
import textwrap

from setuptools import setup  # Import setuptools before distutils
from setuptools._distutils import log

from build_scripts.config import config
from build_scripts.main import get_package_version, get_setuptools_extension_modules
from build_scripts.main import cmd_class_dict
from build_scripts.options import ADDITIONAL_OPTIONS, OPTION
from build_scripts.utils import run_process


class SetupRunner(object):
    def __init__(self, orig_argv):
        self.invocations_list = []

        # Keep the original args around in case we ever need to pass
        # modified arguments to the sub invocations.
        self.orig_argv = orig_argv
        self.sub_argv = list(orig_argv)

        self.setup_script_dir = os.getcwd()

    @staticmethod
    def cmd_line_argument_is_in_args(argument, args):
        """ Check if command line argument was passed in args. """
        return any(arg for arg in list(args) if "--" + argument in arg)

    @staticmethod
    def remove_cmd_line_argument_in_args(argument, args):
        """ Remove command line argument from args. """
        return [arg for arg in list(args) if "--" + argument not in arg]

    @staticmethod
    def construct_cmd_line_argument(name, value=None):
        """ Constructs a command line argument given name and value. """
        if not value:
            return f"--{name}"
        return f"--{name}={value}"

    @staticmethod
    def construct_internal_build_type_cmd_line_argument(internal_build_type):
        return SetupRunner.construct_cmd_line_argument("internal-build-type", internal_build_type)

    def add_setup_internal_invocation(self, build_type, reuse_build=False):
        """ Enqueues a script sub-invocation to be executed later. """
        internal_build_type_arg = self.construct_internal_build_type_cmd_line_argument(build_type)
        setup_cmd = [sys.executable] + self.sub_argv + [internal_build_type_arg]

        command = self.sub_argv[0]
        if command == 'setup.py' and len(self.sub_argv) > 1:
            command = self.sub_argv[1]

        # Add --reuse-build option if requested and not already present.
        if (reuse_build and command in ('bdist_wheel', 'build', 'build_rst_docs', 'install')
                and not self.cmd_line_argument_is_in_args("reuse-build", self.sub_argv)):
            setup_cmd.append(self.construct_cmd_line_argument("reuse-build"))
        self.invocations_list.append(setup_cmd)

    def run_setup(self):
        """
        Decide what kind of build is requested and then execute it.
        In the top-level invocation case, the script
            will spawn setup.py again (possibly multiple times).
        In the internal invocation case, the script
            will run setuptools.setup().
        """

        # Prepare initial config.
        config.init_config(build_type=OPTION["BUILD_TYPE"],
                           internal_build_type=OPTION["INTERNAL_BUILD_TYPE"],
                           cmd_class_dict=cmd_class_dict,
                           package_version=get_package_version(),
                           ext_modules=get_setuptools_extension_modules(),
                           setup_script_dir=self.setup_script_dir,
                           quiet=OPTION["QUIET"])

        # Enable logging for both the top-level invocation of setup.py
        # as well as for child invocations. We we now use
        # setuptools._distutils.log instead of distutils.log, and this
        # new log object does not have its verbosity set by default
        # when setuptools instantiates a distutils Distribution object,
        # which calls
        # dist.parse_command_line() -> log.set_verbosity(self.verbose)
        # on the old distutils log object.
        # So we do it explicitly here.
        if not OPTION["QUIET"]:
            log.set_verbosity(log.INFO)

        # This is an internal invocation of setup.py, so start actual
        # build.
        if config.is_internal_invocation():
            if config.internal_build_type not in config.get_allowed_internal_build_values():
                raise RuntimeError(f"Invalid '{config.internal_build_type}' option given to "
                                   "--internal-build-type. ")
            self.run_setuptools_setup()
            return

        # This is a top-level invocation of setup.py, so figure out what
        # modules we will build and depending on that, call setup.py
        # multiple times with different arguments.
        if config.build_type not in config.get_allowed_top_level_build_values():
            raise RuntimeError(f"Invalid '{config.build_type}' option given to --build-type. ")

        # Build everything: shiboken6, shiboken6-generator and PySide6.
        help_requested = '--help' in self.sub_argv or '-h' in self.sub_argv
        if help_requested:
            self.add_setup_internal_invocation(config.pyside_option_name)

        elif config.is_top_level_build_all():
            self.add_setup_internal_invocation(config.shiboken_module_option_name)

            # Reuse the shiboken build for the generator package instead
            # of rebuilding it again.
            self.add_setup_internal_invocation(config.shiboken_generator_option_name,
                                               reuse_build=True)

            self.add_setup_internal_invocation(config.pyside_option_name)

        elif config.is_top_level_build_shiboken_module():
            self.add_setup_internal_invocation(config.shiboken_module_option_name)

        elif config.is_top_level_build_shiboken_generator():
            self.add_setup_internal_invocation(config.shiboken_generator_option_name)

        elif config.is_top_level_build_pyside():
            self.add_setup_internal_invocation(config.pyside_option_name)

        for cmd in self.invocations_list:
            cmd_as_string = " ".join(cmd)
            exit_code = run_process(cmd)
            if exit_code != 0:
                msg = textwrap.dedent(f"""
                    setup.py invocation failed with exit code: {exit_code}.\n\n
                    setup.py invocation was: {cmd_as_string}
                    """)
                raise RuntimeError(msg)

        if help_requested:
            print(ADDITIONAL_OPTIONS)

    @staticmethod
    def run_setuptools_setup():
        """
        Runs setuptools.setup() once in a single setup.py
        sub-invocation.
        """

        kwargs = config.setup_kwargs
        setup(**kwargs)
