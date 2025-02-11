"""so:

    * some entries in `sys.modules` might not be of type `type(sys)`
    * modules with `.__spec__.origin == 'frozen'` (stateless importer) may have a file
    * some `name, mod in sys.modules.items()` may not respect `name == mod.__name__`
    * `.__name__` may also have be tampered with
    * `.__spec__.origin` might point to a platform library (eg. "[.cpython-platform].so")


other funzies:

    >>> from sys import modules
    >>> modules.clear()
    >>> modules
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    NameError: name 'modules' is not defined
    >>> import sys
    >>> sys.modules
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    AttributeError: module 'sys' has no attribute 'modules'

    >>> type(__builtins__)
    <class 'module'>
    >>> del __builtins__
    >>> type(__builtins__)
    <class 'dict'>

    >>> type(__builtins__)
    <class 'module'>
    >>> from sys import modules
    >>> del sys.modules['__main__']
    >>> type(__builtins__)
    <class 'dict'>

    >>> __builtins__._ = sys.modules
    >>> del _['__main__']
    >>> _['__main__']
    <module '__main__'>

    >>> import os, sys
    >>> type(os.__builtins__)
    <class 'dict'>
    >>> type(__builtins__)
    <class 'module'>
    >>> type(sys.modules['__main__'].__builtins__)
    <class 'module'>
    >>> os.__builtins__ is vars(__builtins__)
    True

"""

import sys


def path(n: str, o: str | None):
    if o in ("frozen", "built-in", None):
        # but truly sys.path, and all of '{/__init__,}.py{c,}' probly
        return f"/usr/lib/python3.12/{n.replace('.', '/')}.py"
    return o or "/oops"


def exists(p: str):
    try:
        open(p).close()
    except FileNotFoundError:
        return False
    else:
        return True


def format(n: str, true_n: str, o: str | None):
    p = path(n, o)
    e = exists(p)
    nn = n if n == true_n else f"{n}({true_n})"
    kind = str(o) if o in ("frozen", "built-in", None) else "(file)"
    kindcol = {"frozen": 34, "built-in": 33}.get(o or "", 37)
    return f"{nn:34}\x1b[{kindcol}m{kind + '\x1b[m':22}\x1b[{31 + e}m{p if e else '-'}\x1b[m"


if "__main__" == __name__:
    for mod in sys.argv[1:]:
        __import__(mod)


    print("\x1b[37msys.module key (true .__name__)   .__spec__.origin   path\x1b[m")
    for n, m in sys.modules.items():
        if type(m) == type(sys) and m.__spec__ is not None:
            print(format(n, m.__name__, m.__spec__.origin))
