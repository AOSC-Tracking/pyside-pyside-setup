
.. _cmake-primer:

************
CMake primer
************

This chapter will is a basic introduction to CMake, the build system
used by PyQtB and the binding generator.

The practical steps will be focusing in using cmake in a unix-like
(GNU/Linux) environment.


Configuring
===========

Project file - CMakeLists.txt
-----------------------------

CMake parses the file CMakeLists.txt for information about the project,
like project name, dependencies, what should be compiled, what should be
shipped.


CMake variables
---------------

CMake can have its default behavior modified by providing some

* ``CMAKE_INSTALL_PREFIX=<some path here>`` sets the install prefix to
  the specified path.
* ``CMAKE_MODULE_PATH=<some path here>`` sets the extra directories
  where CMake will try to find its modules.
* ``CMAKE_TOOLCHAIN_FILE=<file path>`` sets the path to the file that
  describes the toolchain used to compile this project. Is is very useful
  when using CMake with icecc to speedup compilation.

You can define a variable using the ``-D<VARIABLE>`` switch.

Invoking CMake
--------------

After writing the CMakeLists.txt and deciding which flags will be used,
you can invoke CMake using::

  cmake <CMake flags> <path to toplevel CMakeLists.txt file>

For example, if you use the ``build/`` folder to build the project and
want it to be installed into ``/opt/sandbox/``, use the following lines::

  cd build/
  cmake -DCMAKE_INSTALL_PREFIX=/opt/sandbox ..

CMake will process the project file and write the output files in the
current directory

Building
========

After the configuration process, the Makefiles are written and you can build
the project using :program:`make`.

Installing
==========

As in the building process, ``make install`` will install the files into
the target directory.
