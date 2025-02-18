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


def vv(object: object) -> object:
    if isinstance(object, type):
        return object
    if isinstance(object, dict):
        return dict((k, vv(v)) for k, v in object.items())
    if isinstance(object, list):
        return list(vv(e) for e in object)
    if isinstance(object, tuple):
        return tuple(vv(e) for e in object)
    if isinstance(object, set):
        return set(vv(e) for e in object)
    if isinstance(object, frozenset):
        return frozenset(vv(e) for e in object)
    if isinstance(object, slice):
        return slice(vv(object.start), vv(object.stop), vv(object.step))
    try:
        return {"__class__": type(object), **vv(vars(object))}
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

import sys, pdb, bdb

sys.excepthook = lambda ty, val, bt: sys.__excepthook__(ty, val, bt) or ty in {pdb.Restart, bdb.BdbQuit} or pdb.pm()
