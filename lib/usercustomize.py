__all__ = ("colo", "pp", "vv", "bin", "oct", "hex")
import bdb, io, pdb, re, subprocess, sys

_SPECIAL = ('(\'(?:CONNECT|DELETE|GET|HEAD|OPTIONS|PATCH|POST|PUT|TRACE'
                r'|\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2}:\d{2}Z?)?|\d{2}:\d{2}:\d{2}Z?'
                r'|(?:file|https?|ftps?)://.*'
            ')\'|<[^>]*.)')
_PARSE = re.compile(_SPECIAL + (                       # \1     -> (usecase-specific)
                    r'|([\'"])((?:\\\2|.|\n)*?)\2(:)?' # \2\3\2 -> quoted
                    r'|(\d[_\d]*(?:\.\d[_\d]*)?)'      # \4     -> numeral
                    r'|(True|False|None)'              # \5     -> keyword
                    r'|(#.*)'))                        # \6     -> comment
_COLOSCHEME = ['\x1b[36m',       # special (cyan)
               '','\x1b[32m','', # quoted (green)
               '\x1b[33m',       # numeral (yellow)
               '\x1b[34m',       # keyword (blue)
               '\x1b[37m']       # comment (gray)
def colo(rpr: str) -> str:
    def rep(m: re.Match[str]) -> str:
        g = m.groups('')
        k, r = (2+3*len(g[3]), ''.join(g[1]+g[2]+g[1])) if g[1] else next(p for p in enumerate(g) if p[1])
        return _COLOSCHEME[k]+r+'\x1b[m'+g[3]
    return _PARSE.sub(rep, rpr)

def pp(object: object, name: str = "", to: "io.TextIOBase | str" = sys.__stderr__) -> object:
    from pprint import pformat
    ka = {k: not v for k, v in pformat.__kwdefaults__.items() if type(v) is bool}
    ff = name + " = " if name else ""
    ff+= ("\n" + " "*len(ff)).join(colo(l) for l in pformat(object, **ka).splitlines()) + "\n"
    if isinstance(to, str):
        with open(to, "a") as to:
            to.write(ff)
    else:
        to.write(ff)
    return object

def vv(object: object, limit: int = 3) -> object:
    if not limit:                                         return ...
    if isinstance(object, type):                          return object
    if isinstance(object, dict):                          return dict((k, vv(v, limit-1)) for k, v in object.items())
    if isinstance(object, (list, tuple, set, frozenset)): return type(object)(vv(e, limit-1) for e in object)
    if isinstance(object, slice):                         return slice(vv(object.start, limit-1), vv(object.stop, limit-1), vv(object.step, limit-1))
    try:                                                  return {"__class__": type(object), "__dir__": dir(object), **vv(vars(object, limit-1))}
    except TypeError:                                     return object

def _nformat(r: str, width: int = 0, group: int = 0):
    if width: r = r[2:].zfill(width)
    if not group: return r
    l = [r[k - group : k] for k in range(len(r), group - 1, -group)]
    r = r[: len(r) % group]
    r and l.append(r)
    return "_".join(reversed(l))
for _nfunc in {"bin", "oct", "hex"}:
    _nformat
    exec(f"def {_nfunc}(number: int, width: int = 0, group: int = 0) -> str: return _nformat(__{_nfunc}__(number), width, group)")

for k in __all__:
    o = __builtins__.get(k)
    if o: __builtins__[f"__{k}__"] = o
    __builtins__[k] = locals()[k]

pdb.Pdb.do_shell = pdb.Pdb.do_sh = lambda self, arg: print(subprocess.check_output(arg, shell=True).decode(), file=self.stdout, end='')
pdb.Pdb.do_frame = pdb.Pdb.do_f = lambda self, arg: self._select_frame(int(arg) if arg else self.curindex)
pdb.Pdb.do_ed = lambda self, _: subprocess.call("${EDITOR:-ex} '" + self.canonic(self.curframe.f_code.co_filename).replace("'", "'\\''") + f"' +{self.lineno or self.curframe.f_lineno}", shell=True)
sys.excepthook = lambda ty, val, bt: sys.__excepthook__(ty, val, bt) or ty in {pdb.Restart, bdb.BdbQuit} or pdb.pm()
