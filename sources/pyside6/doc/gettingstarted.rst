|project| Getting Started
==========================

.. important:: This page is focused on building |project| **from source**.
  If you just want to install |pymodname|, you need to run: :command:`pip install pyside6`.

  For more details, refer to our `Quick Start`_ guide. Additionally, you can check the
  :ref:`FAQ <faq>` related to the project.

.. _Quick Start: quickstart.html

General Requirements
--------------------

The following prerequisites must be installed before you build |project|.
On **Linux** you might get them with your operating system package manager, on **macOS**
you might get them with ``brew``, and on **Windows** you can download the installer from each
website.

 * **Python**: 3.6+ `[official Python website] <https://www.python.org/downloads/>`_
 * **Qt:** 6.0+ `[online installer] <https://download.qt.io/official_releases/online_installers/>`_
 * **CMake:** 3.18+ `[official CMake website] <https://cmake.org/download/>`_
 * **Git:** 2.0+. `[official Git website] <https://git-scm.com/downloads>`_
 * **libclang:** The libclang library, recommended: version 10 for 6.0+.
   Prebuilt versions for each OS can be `downloaded here`_.

.. _downloaded here: http://download.qt.io/development_releases/prebuilt/libclang/

Guides per platform
-------------------

You can refer to the following pages for platform specific instructions:

.. raw:: html

    <table class="special">
        <colgroup>
            <col style="width: 200px" />
            <col style="width: 200px" />
            <col style="width: 200px" />
        </colgroup>
        <tr>
            <td><a href="gettingstarted-windows.html"><p><strong>Windows</strong></p></a></td>
            <td><a href="gettingstarted-macOS.html"><p><strong>macOS</strong></p></a></td>
            <td><a href="gettingstarted-linux.html"><p><strong>Linux</strong></p></a></td>
        </tr>
    </table>

.. important:: The |project| does not yet support WebAssembly and the mobile operating
      systems (Android or iOS).

  Most Linux-based embedded OS provide PySide with their official
  package manager (for example, `Raspbian`_ and `ArchlinuxARM`_).

.. _Raspbian: https://www.raspbian.org/
.. _ArchlinuxARM: https://archlinuxarm.org/

A normal building command will look like this::

    python setup.py install --qmake=/path/to/qmake \
                            --ignore-git \
                            --debug \
                            --build-tests \
                            --parallel=8 \
                            --verbose-build \
                            --module-subset=Core,Gui,Widgets

Which will build and install the project with **debug** symbols, including the **tests**,
using **ninja** (instead of make), and considering only the **module subset** of
:mod:`QtCore <PySide6.QtCore>`, :mod:`QtGui <PySide6.QtGui>`, and
:mod:`QtWidgets <PySide6.QtWidgets`.

Other important options to consider are:
 * ``--cmake``, to specify the path to the cmake binary,
 * ``--reuse-build``, to rebuild only the modified files,
 * ``--openssl=/path/to/openssl/bin``, to use a different path for OpenSSL,
 * ``--standalone``, to copy over the Qt libraries into the final package to make it work on other
   machines,
 * ``--doc-build-online``, to build documentation using the online template.

Testing the installation
------------------------

Once the installation finishes, you will be able to execute any of our examples::

  python examples/widgets/widgets/tetrix.py

Running Tests
-------------

Using the ``--build-tests`` option will enable us to run all the auto tests inside the project::

  python testrunner.py test > testlog.txt

.. note:: On Windows, don't forget to have qmake in your path
   (:command:`set PATH=E:\\\Path\\\to\\\Qt\\\6.0.0\\\msvc2019_64\\\bin;%PATH%`)

You can also run a specific test (for example ``qpainter_test``) by running::

    ctest -R qpainter_test --verbose

Building the documentation
--------------------------

Starting from 5.15, there are two options to build the documentation:

1. Building rst-only documentation (no API)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The process of parsing Qt headers to generate the PySide API documentation can take several
minutes, this means that modifying a specific section of the rst files we currently have, might
become a hard task.

For this, you can install :command:`sphinx` on a virtual environment, and execute the following command::

    python setup.py build_rst_docs

which will generate a ``html/`` directory with the following structure::

    html
    └── pyside6
        ├── index.html
        ├── ...
        └── shiboken6
            ├── index.html
            └── ...

so you can open the main page ``html/pyside6/index.html`` on your browser to check the generated
files.

This is useful when updating the general sections of the documentation, adding tutorials,
modifying the build instructions, and more.

2. Building the documentation (rst + API)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The documentation is being generated using **qdoc** to get the API information, and also **sphinx**
for the local Python related notes.

The system required ``libxml2`` and ``libxslt``, also on the Python environment, ``sphinx`` and
``graphviz`` need to be installed before running the installation process::

    pip install graphviz sphinx sphinx_tabs

After installing ``graphviz``, the ``dot`` command needs to be in PATH, otherwise,
the process will fail. Installing ``graphviz`` system-wide is also an option.

Since the process rely on a Qt installation, you need to specify where the ``qtbase`` directory
you will use with your ``qmake`` is located::

    export QT_SRC_DIR=/path/to/qtbase

Once the build process finishes, you can go to the generated ``*_build/*_release/pyside6``
directory, and run::

    ninja apidoc

.. note:: The :command:`apidoc` make target builds offline documentation in QCH (Qt Creator Help)
   format by default. You can switch to building for the online use with the ``--doc-build-online``
   configure option.

The target executes several steps:

#. The ``qdoc`` tool is run over the Qt source code to produce documentation in WebXML format.
#. ``shiboken6`` is run to extract the functions for which bindings exist from WebXML and convert it into RST.
#. ``sphinx`` is run to produce the documentation in HTML format.

Re-running the command will not execute step 1 unless the file
``qdoc_output/webxml/qtcore-index.webxml`` is removed from the build tree.
Similarly, step 2 will not be executed unless the file ``rst/PySide6/QtCore/index.rst``
is removed.

Finally, you will get a ``html`` directory containing all the generated documentation. The offline
help files, ``PySide.qch`` and ``Shiboken.qch``, can be moved to any directory of your choice. You
can find ``Shiboken.qch`` in the build directory, ``*_build\*_release\shiboken6\doc\html``.

If you want to temporarily change a ``.rst`` file to examine the impact on
formatting, you can re-run ``sphinx`` in the ``doc`` directory::

    sphinx-build rst html

Viewing offline documentation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The offline documentation (QCH) can be viewed using the Qt Creator IDE or Qt Assistant, which is
a standalone application for viewing QCH files.

To view the QCH using Qt Creator, following the instructions outlined in
`Using Qt Creator Help Mode <https://doc.qt.io/qtcreator/creator-help.html>`_. If you chose to
use Qt Assistant instead, use the following command to register the QCH file before launching
Qt Assistant::

    assistant -register PySide.qch

.. note:: Qt Assistant renders the QCH content using the QTextBrowser backend, which supports
   a subset of the CSS styles, However, Qt Creator offers an alternative litehtml-based
   backend, which offers better browsing experience. At the moment, this is not the default
   backend, so you have to select the litehtml backend
   explicitly under the ``General`` tab in ``Qt Creator >> Tools >> Options >> Help``.

Using the internal tools
------------------------

A set of tools can be found under the ``tools/`` directory inside the ``pyside-setup`` repository.

* ``checklibs.py``: Script to analyze dynamic library dependencies of Mach-O binaries.
  To use this utility, just run::

    python checklibs.py /path/to/some.app/Contents/MacOS/Some

  This script was fetched from this repository_.

* ``create_changelog.py``: Script used to create the CHANGELOG that you can find in the ``dist/``
  directory. Usage::

    python create_changelog.py -r 6.0.1 -v v6.0.0..6.0 -t bug-fix

* ``debug_windows.py``: This script can be used to find out why PySide modules
  fail to load with various DLL errors like Missing DLL or Missing symbol in DLL.

  You can think of it as a Windows version of :command:`ldd` / ``LD_DEBUG``.

  Underneath, it uses the :command:`cdb.exe` command line debugger and the :command:`gflags.exe`
  tool, which are installed with the latest Windows Kit.

  The aim is to help developers debug issues that they may encounter using the PySide imports on
  Windows. The user should then provide the generated log file.

  Incidentally it can also be used for any Windows executables, not just Python.
  To use it just run::

    python debug_windows.py

* ``missing_bindings.py``: This script is used to compare the state of PySide and PyQt
  regarding available modules and classses. This content is displayed in our `wiki page`_,
  and can be used as follows::

    python missing_bindings.py --qt-version 6.0.1 -w all

.. note:: The script relies on BeautifulSoup to parse the content and generate a list of the
   missing bindings.

.. _repository: https://github.com/liyanage/macosx-shell-scripts/
.. _`wiki page`: https://wiki.qt.io/Qt_for_Python_Missing_Bindings
.. _BeautifulSoup: https://www.crummy.com/software/BeautifulSoup/
