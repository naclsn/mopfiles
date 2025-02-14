from pprint import pprint as _pprint


def pp(object: object, *a: ..., name: str = '_', **ka: ...):
    ka = (
        {}
        if sys.version_info < (3, 8)
        else {"compact": 1, "sort_dicts": 0, "underscore_numbers": 1, **ka}
    )
    if a and str == type(a[0]):
        name, *a = a
    if name:
        print(name, '=', end=' ')
    _pprint(object, *a, **ka)
    return object


def vars(object: object, rec: bool = False) -> object:
    if not rec:
        return __vars__(object)
    if isinstance(object, type):
        return object
    if isinstance(object, dict):
        return dict((k, vars(v, rec)) for k, v in object.items())
    if isinstance(object, list):
        return list(vars(e, rec) for e in object)
    if isinstance(object, tuple):
        return tuple(vars(e, rec) for e in object)
    if isinstance(object, set):
        return set(vars(e, rec) for e in object)
    if isinstance(object, frozenset):
        return frozenset(vars(e, rec) for e in object)
    if isinstance(object, slice):
        sss = object.start, object.stop, object.step
        return slice(*(vars(e, rec) for e in sss))
    try:
        return {"__class__": type(object), **vars(__vars__(object), rec)}
    except TypeError:
        return object


def _nformat(r: str, width: int = 0, group: int = 0):
    if width:
        r = r[2:].zfill(width)
    if not group:
        return r
    l = [r[k - group : k] for k in range(len(r), group - 1, -group)]
    r = r[: len(r) % group]
    if r:
        l.append(r)
    return "_".join(reversed(l))


for _nfunc in ("bin", "oct", "hex"):
    _nformat
    exec(
        f"""def {_nfunc}(number: int, width: int = 0, group: int = 0):
        return _nformat(__{_nfunc}__(number), width, group)"""
    )

for k, v in list(locals().items()):
    if "_" != k[0]:
        o = __builtins__.get(k)
        if o:
            __builtins__[f"__{k}__"] = o
        __builtins__[k] = v

import sys, pdb

sys.excepthook = lambda *_: sys.__excepthook__(*_) or pdb.pm()
