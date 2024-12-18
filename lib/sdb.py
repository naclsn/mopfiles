"""Socket DB

Usage::
    import sdb; sdb.config(...)
    ...
    breakpoint()

Here is an alternative one-line to use with FIFOs::
    import sys;s=vars(sys);b='breakpointhook';s[b]=lambda:s.update(s[n].close()or(n,open("/tmp/"+n,s[n].mode))for n in('stdin','stdout'))or s[f'__{b}__']()

    mkfifo /tmp/stdin /tmp/stdout
    cat /tmp/stdout & cat >/tmp/stdin
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

    def __post_init__(self):
        global _config
        _config = self
        sys.breakpointhook = set_trace


del dataclass
_config = None


def set_trace(*a: ..., **ka: ...):
    _config or config()

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

    stdio = sys.stdin, sys.stdout
    sys.stdin = sys.stdout = io
    try:
        return sys.__breakpointhook__(*a, **ka)
    finally:
        sys.stdin, sys.stdout = stdio
        io.close()
