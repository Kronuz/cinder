# Portions copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
from .consts import PyCF_ONLY_AST, PyCF_DONT_IMPLY_DEDENT

CO_FUTURE_DIVISION = 0x20000
CO_FUTURE_ABSOLUTE_IMPORT = 0x40000
CO_FUTURE_WITH_STATEMENT = 0x80000
CO_FUTURE_PRINT_FUNCTION = 0x100000
CO_FUTURE_UNICODE_LITERALS = 0x200000
CO_FUTURE_BARRY_AS_BDFL = 0x400000
CO_FUTURE_GENERATOR_STOP = 0x800000
CO_FUTURE_ANNOTATIONS = 0x1000000
CO_STATICALLY_COMPILED = 0x4000000
PyCF_TYPE_COMMENTS = 0x1000
PyCF_ALLOW_TOP_LEVEL_AWAIT = 0x2000
CO_SHADOW_FRAME = 0x10000000
CO_NORMAL_FRAME = 0x20000000
CO_SUPPRESS_JIT = 0x40000000
PyCF_COMPILE_MASK: int = (
    PyCF_ONLY_AST
    | PyCF_ALLOW_TOP_LEVEL_AWAIT
    | PyCF_TYPE_COMMENTS
    | PyCF_DONT_IMPLY_DEDENT
)

PyCF_MASK: int = (
    CO_FUTURE_DIVISION
    | CO_FUTURE_ABSOLUTE_IMPORT
    | CO_FUTURE_WITH_STATEMENT
    | CO_FUTURE_PRINT_FUNCTION
    | CO_FUTURE_UNICODE_LITERALS
    | CO_FUTURE_BARRY_AS_BDFL
    | CO_FUTURE_GENERATOR_STOP
    | CO_FUTURE_ANNOTATIONS
    | CO_STATICALLY_COMPILED
    | CO_SHADOW_FRAME
)
