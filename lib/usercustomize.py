from pprint import pprint as _pprint


def pp(object: object, *a: ..., **ka: ...):
    ka = {"compact": 1, "sort_dicts": 0, "underscore_numbers": 1} | ka
    _pprint(object, *a, **ka)
    return object


def _nformat(r: str, width: int = 0, group: int = 0, /):
    if width:
        r = ("0" * width + r[2:])[-width:]
    if not group:
        return r
    l = [r[k - group : k] for k in range(len(r), group - 1, -group)]
    if r := r[: len(r) % group]:
        l.append(r)
    return "_".join(reversed(l))


for _nfunc in ("bin", "oct", "hex"):
    exec(
        f"""def {_nfunc}(number: int, width: int = 0, group: int = 0, /):
        return _nformat(__{_nfunc}__(number), width, group)"""
    )

for k, v in list(locals().items()):
    if "_" != k[0]:
        if o := __builtins__.get(k):
            __builtins__[f"__{k}__"] = o
        __builtins__[k] = v


import sys, pdb

sys.excepthook = lambda *_: sys.__excepthook__(*_) or pdb.pm()
