class whatever:
    """Print whatever is done to itself"""

    _instances: "dict[int, str]" = {}

    from builtins import print
    from pprint import pprint

    def __init__(self, tag: str):
        whatever._instances[id(self)] = tag

    for dunder, ret in (
        ("__abs__", None),
        ("__add__", None),
        ("__aenter__", None),  # XXX?
        ("__aexit__", None),  # XXX?
        ("__aiter__", None),  # XXX?
        ("__and__", None),
        ("__anext__", None),  # XXX?
        ("__await__", None),
        ("__bool__", True),  # YYY
        ("__buffer__", memoryview(b"C")),
        ("__bytes__", b"B"),  # YYY
        ("__call__", None),
        ("__ceil__", None),
        ("__class_getitem__", None),
        ("__complex__", None),
        ("__contains__", None),
        ("__del__", None),
        ("__delattr__", None),
        ("__delete__", None),
        ("__delitem__", None),
        ("__dir__", []),  # YYY
        ("__divmod__", None),
        ("__enter__", None),
        ("__eq__", None),
        ("__exit__", None),
        ("__float__", None),
        ("__floor__", None),
        ("__floordiv__", None),
        ("__format__", None),
        ("__ge__", None),
        ("__get__", None),
        ("__getattr__", None),
        ("__getattribute__", None),
        ("__getitem__", None),
        ("__gt__", None),
        ("__hash__", None),
        ("__iadd__", None),
        ("__iand__", None),
        ("__ifloordiv__", None),
        ("__ilshift__", None),
        ("__imatmul__", None),
        ("__imod__", None),
        ("__imul__", None),
        ("__index__", -1),  # YYY
        ("__init_subclass__", None),
        ("__instancecheck__", None),
        ("__int__", None),
        ("__invert__", None),
        ("__ior__", None),
        ("__ipow__", None),
        ("__irshift__", None),
        ("__isub__", None),
        ("__iter__", None),
        ("__itruediv__", None),
        ("__ixor__", None),
        ("__le__", None),
        ("__len__", None),
        ("__length_hint__", None),
        ("__lshift__", None),
        ("__lt__", None),
        ("__match_args__", None),
        ("__matmul__", None),
        ("__missing__", None),
        ("__mod__", None),
        ("__mul__", None),
        ("__ne__", None),
        ("__neg__", None),
        ("__or__", None),
        ("__pos__", None),
        ("__pow__", None),
        ("__radd__", None),
        ("__rand__", None),
        ("__rdivmod__", None),
        ("__release_buffer__", None),
        ("__repr__", "(whatever)"),  # YYY
        ("__reversed__", None),
        ("__rfloordiv__", None),
        ("__rlshift__", None),
        ("__rmatmul__", None),
        ("__rmod__", None),
        ("__rmul__", None),
        ("__ror__", None),
        ("__round__", None),
        ("__rpow__", None),
        ("__rrshift__", None),
        ("__rshift__", None),
        ("__rsub__", None),
        ("__rtruediv__", None),
        ("__rxor__", None),
        ("__set__", None),
        ("__set_name__", None),
        ("__setattr__", None),
        ("__setitem__", None),
        ("__str__", "A"),  # YYY
        ("__sub__", None),
        ("__subclasscheck__", None),
        ("__truediv__", None),
        ("__trunc__", None),
        ("__xor__", None),
    ):
        exec(
            f"""\
def {dunder}(self, *a, **ka):
    name = whatever._instances[id(self)]
    whatever.print(f"/------ {{name}}.{dunder}({{id(self)}}, ..)")
    a and whatever.pprint(a)
    ka and whatever.pprint(ka)
    whatever.print(" ------/")
    return {"self" if ret is None else repr(ret)}
"""
        )
