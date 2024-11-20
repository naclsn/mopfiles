"""TODO: short doc"""

import importlib
import os
import signal
import socket
import sys
import threading

_instance = None
_lock = threading.Lock()


def _rtdb(cls: type, srv: socket.socket, frame: "object | None"):
    conn = srv.accept()[0]
    io = conn.makefile("rw")
    close = lambda: (io.close(), conn.close(), srv.close())

    global _instance
    with _lock:
        _instance = cls(stdin=io, stdout=io)
    _instance.postloop = close
    _instance.set_trace(frame)


def _rtdb_hookfn():
    if not _lock.acquire(blocking=False):
        return

    if _instance is not None:
        _lock.release()
        return

    global _instance
    _instance = object()  # placeholder

    frame = sys._getframe(2) if hasattr(sys, "_getframe") else None
    print(
        f"Breakpoint call {frame.f_code.co_filename}:{frame.f_lineno}"
        if frame
        else "Breakpoint call reached somewhere"
    )

    srv = socket.socket()
    try:
        module = os.getenv("PYTHONBREAKPOINT", "pdb.set_trace")[:-10]
        cls = importlib.import_module(module).Pdb

        port = int(os.getenv("PYTHONBREAKPOINT_PORT", 4444))
        srv.bind(("localhost", port))
        srv.listen(1)

        th = threading.Thread(target=_rtdb, args=(cls, srv, frame))
        th.start()
        print(f"Debugger listening on {port} (using {module})")

    except Exception as e:
        print(f"Could not start debugger: {e}")
        srv.close()
        _instance = None

    finally:
        _lock.release()


def enable_rtdb():
    sys.breakpointhook = _rtdb_hookfn
    signal.signal(signal.SIGUSR1, lambda _sig, _frame: breakpoint())
    return os.getpid()


# ---

with open("/tmp/wip.pid", "w") as pid:
    print(enable_rtdb(), file=pid)

# ---

hello = 0
while True:
    import time

    print("hello", hello)
    time.sleep(1)
    hello += 1
