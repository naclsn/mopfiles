import dataclasses
import typing


_dataclass = dataclasses.dataclass


def _strictdataclass(cls=None, **kwargs):
    if cls is None:
        return lambda cls: _strictdataclass(cls, **kwargs)

    r = _dataclass(cls, **kwargs)
    post = getattr(cls, "__post_init__", None)

    def __post_init__(self, *args, **kwargs):
        if post:
            post(self, *args, **kwargs)
        for attr, ty in typing.get_type_hints(cls).items():
            assert isinstance(getattr(self, attr), ty)

    cls.__post_init__ = __post_init__
    return r


dataclasses.dataclass = _strictdataclass
