__all__ = ("pp", "vv", "bin", "oct", "hex")
import bdb, io, pdb, subprocess, sys


def pp(object: object, name: str = "_", to: "io.TextIOBase | str" = sys.__stderr__):
    from pprint import pprint as _pprint
    ka = {k: not v for k, v in _pprint.__kwdefaults__.items() if type(v) is bool}

    if isinstance(to, str):
        with open(to, "a") as to:
            name and print(name, "=", end=" ", file=to)
            _pprint(object, to, **ka)
    else:
        name and print(name, "=", end=" ", file=to)
        _pprint(object, to, **ka)
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
    if width: r = r[2:].zfill(width)
    if not group: return r
    l = [r[k - group : k] for k in range(len(r), group - 1, -group)]
    r = r[: len(r) % group]
    r and l.append(r)
    return "_".join(reversed(l))

for _nfunc in ("bin", "oct", "hex"):
    _nformat
    exec(
        f"""def {_nfunc}(number: int, width: int = 0, group: int = 0):
        return _nformat(__{_nfunc}__(number), width, group)"""
    )


for k in __all__:
    o = __builtins__.get(k)
    if o: __builtins__[f"__{k}__"] = o
    __builtins__[k] = locals()[k]


pdb.Pdb.do_shell = pdb.Pdb.do_sh = lambda self, arg: print(subprocess.check_output(arg, shell=True).decode(), file=self.stdout, end='')
pdb.Pdb.do_frame = pdb.Pdb.do_f = lambda self, arg: self._select_frame(int(arg) if arg else self.curindex)
pdb.Pdb.do_ed = lambda self, _: subprocess.call("${EDITOR:-ex} '" + self.canonic(self.curframe.f_code.co_filename).replace("'", "'\\''") + f"' +{self.lineno or self.curframe.f_lineno}", shell=True)

sys.excepthook = lambda ty, val, bt: sys.__excepthook__(ty, val, bt) or ty in {pdb.Restart, bdb.BdbQuit} or pdb.pm()
