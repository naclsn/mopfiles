"""Socket debug

Usage::
    import sdb; sdb.config(...)
    ...
    breakpoint()

    $ rlwrap nc localhost 4099

Here is an alternative one-line to use with FIFOs::
    import sys;s=vars(sys);b="breakpointhook";f="stdin","stdout","stderr";s[b]=lambda:s.update({n:open("/tmp/"+n,s[n].mode)for n in f})or s[f"__{b}__"]()or s.update({n:s[n].close()or s[f"__{n}__"]for n in f+(b,)})

    $ mkfifo /tmp/std{in,out,err}
    $ cat /tmp/stdout & cat /tmp/stderr & >/tmp/stdin python -c 'while 1:import readline;print(input("(Pdb) "))'
"""


__all__ = ["config", "set_trace", "socket", "sys"]

import socket
import sys

from dataclasses import dataclass


@dataclass
class config:
    socket_family: int = -1
    socket_type: int = -1
    socket_address: "tuple[str, int] | str" = ("localhost", 4099)
    grab_stdio: bool = False
    use_file: "str | None" = None
    set_sys_breakpointhook: bool = True

    def __post_init__(self):
        global _config
        _config = self
        if self.set_sys_breakpointhook:
            sys.breakpointhook = set_trace


del dataclass
_config = None


def set_trace(*a: ..., **ka: ...):
    _config or config(set_sys_breakpointhook=False)
    assert _config

    if _config.use_file:
        try:
            with open(_config.use_file) as f:
                import json

                config(**json.load(f))
        except FileNotFoundError:
            return

    srv = socket.socket(_config.socket_family, _config.socket_type)
    srv.bind(_config.socket_address)
    srv.listen(1)
    conn = srv.accept()[0]
    srv.close()
    io = conn.makefile("rw")
    conn.close()

    stdio = sys.stdin, sys.stdout, sys.stderr
    sys.stdin = sys.stdout = sys.stderr = io
    try:
        return sys.__breakpointhook__(*a, **ka)
    finally:
        sys.stdin, sys.stdout, sys.stderr = stdio
        io.close()
