"""TODO: short doc"""

import importlib
import os
import signal
import socket
import sys
import threading

_instance = None
_lock = threading.Lock()

# TODO: I think the connection got closed when doing `(Pdb) n`


def _rtdb(cls: type, srv: socket.socket, frame: "object | None", mt: bool):
    conn, addr = srv.accept()
    io = conn.makefile("rw")
    print(f"Connection established with {addr}")

    def close():
        print("Connection terminated")

        io.close()
        conn.close()
        srv.close()

        global _instance
        if mt:
            with _lock:
                _instance = None
        else:
            _instance = None

    try:
        db = cls(stdin=io, stdout=io)
        db.postloop = close

        global _instance
        if mt:
            with _lock:
                _instance = db
        else:
            _instance = db

        db.set_trace(frame)

    except Exception as e:
        print(f"Could not start debugger: {e}")
        close()


def _rtdb_hookfn():
    if not _lock.acquire(blocking=False):
        return

    global _instance
    if _instance is not None:
        _lock.release()
        return
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

        mt = bool(os.getenv("PYTHONBREAKPOINT_MT", False))

        args = (cls, srv, frame, mt)
        if mt:
            threading.Thread(target=_rtdb, args=args).start()
        else:
            _rtdb(*args)
        print(f"Debugger listening on {port} (using {module})")

    except Exception as e:
        print(f"Could not setup debugging: {e}")
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

import common
import time

hello = 0
while True:
    print("hello", hello, common.a)
    time.sleep(1)
    hello += 1
