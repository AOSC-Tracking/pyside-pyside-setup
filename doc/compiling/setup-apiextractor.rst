
.. _api-extractor:

**************
API Extractor
**************

Overview
========

The **API Extractor** library is used by the binding generator to
parse the header and typesystem files to create an internal
representation of the API. It is based on the QtScriptGenerator
codebase.

Getting the sources
===================

* Download URL: http://www.pyside.org/downloads/

Build requirements
==================

* Qt4.5 development headers and libraries >= 4.5.0
* libboost-graph >= 1.38.0
* cmake >= 2.6.0

Building and installing
=======================

To build and install just follow the generic cmake instructions in section
:ref:`cmake-primer`.

Debian packaging
================

In order to compile this package in a debian environment, make sure the
following packages are installed:

* debhelper (>= 5)
* cdbs
* cmake (>= 2.6.0)
* libboost-graph1.38-dev (>= 1.38.0)
* libqt4-dev (>= 4.5)

And then you can build the package using::

  $ dpkg-buildpackage -rfakeroot
