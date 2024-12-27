from dataclasses import dataclass


class _dc(type):
    _w = dataclass(frozen=True, slots=True)

    def __new__(cls, name: str, bases: tuple[type, ...], dict: dict[str, object]):
        r = type.__new__(cls, name, bases, dict)
        return r if "__dataclass_params__" in dict else _dc._w(r)
