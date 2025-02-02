# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
from __future__ import annotations

"""
layout.py

The signature module now has the capability to configure
differently formatted versions of signatures. The default
layout is known from the "__signature__" attribute.

The function "get_signature(ob, modifier=None)" produces the same
signatures by default. By passing different modifiers, you
can select different layouts.

This module configures the different layouts which can be used.
It also implements them in this file. The configurations are
used literally as strings like "signature", "existence", etc.
"""

import inspect
import typing

from types import SimpleNamespace
from textwrap import dedent
from shibokensupport.signature.mapping import ellipsis


class SignatureLayout(SimpleNamespace):
    """
    Configure a signature.

    The layout of signatures can have different layouts which are
    controlled by keyword arguments:

    definition=True         Determines if self will generated.
    defaults=True
    ellipsis=False          Replaces defaults by "...".
    return_annotation=True
    parameter_names=True    False removes names before ":".
    """
    allowed_keys = SimpleNamespace(definition=True,
                                   defaults=True,
                                   ellipsis=False,
                                   return_annotation=True,
                                   parameter_names=True)
    allowed_values = True, False

    def __init__(self, **kwds):
        args = SimpleNamespace(**self.allowed_keys.__dict__)
        args.__dict__.update(kwds)
        self.__dict__.update(args.__dict__)
        err_keys = list(set(self.__dict__) - set(self.allowed_keys.__dict__))
        if err_keys:
            self._attributeerror(err_keys)
        err_values = list(set(self.__dict__.values()) - set(self.allowed_values))
        if err_values:
            self._valueerror(err_values)

    def __setattr__(self, key, value):
        if key not in self.allowed_keys.__dict__:
            self._attributeerror([key])
        if value not in self.allowed_values:
            self._valueerror([value])
        self.__dict__[key] = value

    def _attributeerror(self, err_keys):
        err_keys = ", ".join(err_keys)
        allowed_keys = ", ".join(self.allowed_keys.__dict__.keys())
        raise AttributeError(dedent(f"""\
            Not allowed: '{err_keys}'.
            The only allowed keywords are '{allowed_keys}'.
            """))

    def _valueerror(self, err_values):
        err_values = ", ".join(map(str, err_values))
        allowed_values = ", ".join(map(str, self.allowed_values))
        raise ValueError(dedent(f"""\
            Not allowed: '{err_values}'.
            The only allowed values are '{allowed_values}'.
            """))


# The following names are used literally in this module.
# This way, we avoid the dict hashing problem.
signature = SignatureLayout()

existence = SignatureLayout(definition=False,
                            defaults=False,
                            return_annotation=False,
                            parameter_names=False)

hintingstub = SignatureLayout(ellipsis=True)

typeerror = SignatureLayout(definition=False,
                            return_annotation=False,
                            parameter_names=False)


def define_nameless_parameter():
    """
    Create Nameless Parameters

    A nameless parameter has a reduced string representation.
    This is done by cloning the parameter type and overwriting its
    __str__ method. The inner structure is still a valid parameter.
    """
    def __str__(self):
        # for Python 2, we must change self to be an instance of P
        klass = self.__class__
        self.__class__ = P
        txt = P.__str__(self)
        self.__class__ = klass
        txt = txt[txt.index(":") + 1:].strip() if ":" in txt else txt
        return txt

    P = inspect.Parameter
    newname = "NamelessParameter"
    bases = P.__bases__
    body = dict(P.__dict__)  # get rid of mappingproxy
    if "__slots__" in body:
        # __slots__ would create duplicates
        for name in body["__slots__"]:
            del body[name]
    body["__str__"] = __str__
    return type(newname, bases, body)


NamelessParameter = define_nameless_parameter()

"""
Note on the "Optional" feature:

When an annotation has a default value that is None, then the
type has to be wrapped into "typing.Optional".

Note that only the None value creates an Optional expression,
because the None leaves the domain of the variable.
Defaults like integer values are ignored: They stay in the domain.

That information would be lost when we use the "..." convention.

Note that the typing module has the remarkable expansion

    Optional[T]    is    Union[T, NoneType]

We want to avoid that when generating the .pyi file.
This is done by a regex in pyi_generator.py .
The following would work in Python 3, but this is a version-dependent
hack that also won't work in Python 2 and would be _very_ complex.
"""
# import sys
# if sys.version_info[0] == 3:
#     class hugo(list):pass
#     typing._normalize_alias["hugo"] = "Optional"
#     Optional = typing._alias(hugo, typing.T, inst=False)
# else:
#     Optional = typing.Optional


def make_signature_nameless(signature):
    """
    Make a Signature Nameless

    We use an existing signature and change the type of its parameters.
    The signature looks different, but is totally intact.
    """
    for key in signature.parameters.keys():
        signature.parameters[key].__class__ = NamelessParameter


_POSITIONAL_ONLY         = inspect._POSITIONAL_ONLY  # noqa E:201
_POSITIONAL_OR_KEYWORD   = inspect._POSITIONAL_OR_KEYWORD  # noqa E:201
_VAR_POSITIONAL          = inspect._VAR_POSITIONAL  # noqa E:201
_KEYWORD_ONLY            = inspect._KEYWORD_ONLY  # noqa E:201
_VAR_KEYWORD             = inspect._VAR_KEYWORD  # noqa E:201
_empty                   = inspect._empty  # noqa E:201


def create_signature(props, key):
    if not props:
        # empty signatures string
        return
    if isinstance(props["multi"], list):
        # multi sig: call recursively.
        # PYSIDE-2846: Fold duplicate signatures away
        res = list(set(list(create_signature(elem, key)
                            for elem in props["multi"])))
        return res if len(res) > 1 else res[0]

    if type(key) is tuple:
        _, modifier = key
    else:
        _, modifier = key, "signature"

    layout = globals()[modifier]  # lookup of the modifier in this module
    if not isinstance(layout, SignatureLayout):
        raise SystemError("Modifiers must be names of a SignatureLayout "
                          "instance")

    # this is the basic layout of a signature
    varnames = props["varnames"]
    if layout.definition:
        # PYSIDE-1328: We no longer use info from the sig_kind which is
        # more complex for multiple signatures. We now get `self` from the
        # parser.
        pass
    else:
        if varnames and varnames[0] in ("self", "cls"):
            varnames = varnames[1:]

    # calculate the modifications
    defaults = props["defaults"][:]
    if not layout.defaults:
        defaults = ()
    annotations = props["annotations"].copy()
    if not layout.return_annotation and "return" in annotations:
        del annotations["return"]

    # Build a signature.
    kind = inspect._POSITIONAL_OR_KEYWORD
    params = []
    for idx, name in enumerate(varnames):
        if name.startswith("**"):
            kind = _VAR_KEYWORD
        elif name.startswith("*"):
            kind = _VAR_POSITIONAL
        ann = annotations.get(name, _empty)
        if ann in ("self", "cls"):
            ann = _empty
        name = name.lstrip("*")
        defpos = idx - len(varnames) + len(defaults)
        default = defaults[defpos] if defpos >= 0 else _empty
        if default is None:
            ann = typing.Optional[ann]
        if default is not _empty and layout.ellipsis:
            default = ellipsis
        param = inspect.Parameter(name, kind, annotation=ann, default=default)
        params.append(param)
        if kind == _VAR_POSITIONAL:
            kind = _KEYWORD_ONLY
    sig = inspect.Signature(params,
                            return_annotation=annotations.get('return', _empty),
                            __validate_parameters__=False)

    # the special case of nameless parameters
    if not layout.parameter_names:
        make_signature_nameless(sig)
    return sig

# end of file
