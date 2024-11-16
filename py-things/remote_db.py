"""TODO: short doc"""

import importlib
import os
import signal
import socket
import sys


def _remote_db():
    """
    DO NO TOUCH ANYTHING without first seeing with me.

    Everything in here is very minute and removing or adding a seemingly
    unrelated line can mess with the debugger somehow.

    TODO/FIXME:
    * I think using the command `run` crashes the application.
      (last time I tested, `pdb.Restart` wasn't caught by pdb like it ought to)
    * Other thing is it still gave me some "[Errno 98] Address already in use"
      after (supposedly) clean shutdowns...
    """

    srv, conn, io = None, None, None
    close = lambda: any(r.close() for r in (io, conn, srv) if r)

    try:
        module = os.getenv("PYTHONBREAKPOINT", "pdb.set_trace")[:-10]
        port = int(os.getenv("PYTHONBREAKPOINT_PORT", 4444))

        frame = sys._getframe(2) if hasattr(sys, "_getframe") else None
        print(
            f"Breakpoint call {frame.f_code.co_filename}:{frame.f_lineno}"
            if frame
            else "Breakpoint call reached"
        )

        cls = importlib.import_module(module).Pdb
        srv = socket.socket()
        srv.bind(("localhost", port))
        srv.listen(1)

        print(f"Debugger listening on {port} (using {module})")
        conn = srv.accept()[0]
        io = conn.makefile("rw")

        db = cls(stdin=io, stdout=io)
        db.postloop = close
        db.set_trace(frame)

    except Exception as e:
        print(f"Could not start debugger: {e}")
        close()


def enable_remote_db():
    """
    TODO: short doc

    # eg. export PYTHONBREAKPOINT=IPython.core.debugger.set_trace
    """
    sys.breakpointhook = _remote_db
    signal.signal(signal.SIGUSR1, lambda _sig, _frame: breakpoint())
    return os.getpid()


# ---

with open("/tmp/wip.pid", "w") as pid:
    print(enable_remote_db(), file=pid)

# ---

hello = 0
while True:
    import time

    time.sleep(0.5)
    hello += 1
