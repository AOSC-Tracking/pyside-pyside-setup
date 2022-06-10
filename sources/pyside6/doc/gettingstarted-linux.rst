Getting Started on Linux
==========================

Requirements
------------

 * GCC
 * ``sphinx`` package for the documentation (optional).
 * Depending on your linux distribution, the following dependencies might also be required:

    * ``libgl-dev``, ``python-dev``, ``python-distutils``, and ``python-setuptools``.

Building from source
--------------------

Creating a virtual environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``venv`` module allows you to create a local, user-writeable copy of a python environment into
which arbitrary modules can be installed and which can be removed after use::

    python -m venv testenv
    source testenv/bin/activate
    pip install -r requirements.txt  # General dependencies, documentation, and examples.

will create and use a new virtual environment, which is indicated by the command prompt changing.

Setting up CLANG
~~~~~~~~~~~~~~~~

If you don't have libclang already in your system, you can download from the Qt servers::

    wget https://download.qt.io/development_releases/prebuilt/libclang/libclang-release_100-based-linux-Rhel7.6-gcc5.3-x86_64.7z

Extract the files, and leave it on any desired path, and set the environment
variable required::

    7z x libclang-release_100-based-linux-Rhel7.6-gcc5.3-x86_64.7z
    export LLVM_INSTALL_DIR=$PWD/libclang

Getting PySide
~~~~~~~~~~~~~~

Cloning the official repository can be done by::

    git clone --recursive https://code.qt.io/pyside/pyside-setup

Checking out the version that we want to build, for example 6.0::

    cd pyside-setup && git checkout 6.0

.. note:: Keep in mind you need to use the same version as your Qt installation.
          Additionally, :command:`git checkout -b 6.0 --track origin/6.0` could be a better option
          in case you want to work on it.

Building PySide
~~~~~~~~~~~~~~~

Check your Qt installation path, to specifically use that version of qtpaths to build PySide.
for example, :command:`/opt/Qt/6.0.0/gcc_64/bin/qtpaths`.

Build can take a few minutes, so it is recommended to use more than one CPU core::

    python setup.py build --qtpaths=/opt/Qt/6.0.0/gcc_64/bin/qtpaths --build-tests --ignore-git --parallel=8

Installing PySide
~~~~~~~~~~~~~~~~~

To install on the current directory, just run::

    python setup.py install --qtpaths=/opt/Qt/6.0.0/gcc_64/bin/qtpaths --build-tests --ignore-git --parallel=8

Test installation
~~~~~~~~~~~~~~~~~

You can execute one of the examples to verify the process is properly working.
Remember to properly set the environment variables for Qt and PySide::

    python examples/widgets/widgets/tetrix.py
