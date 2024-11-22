"""
Threaded Remote Read Eval Print Loop (trrepl - read "triple")

TODO/COULDO: provide a basic completion-like for use with eg rlwrap
"""

import enum
import os
import pprint
import signal
import socket
import sys
import threading
import types


class trrepl:
    """
    TODO: make `help(trrepl())` useful

    potential 'setting-like's:
    - do notify with print  :g/[^.]print(
    - set self as globals("__"), instead of trrepl()
    """

    _instance = None
    _lock = threading.Lock()

    class States(enum.Enum):
        READLINE = "<<< "
        CONTINUE = ",,, "
        DONE = "?!"

    def __new__(cls, *a: ..., **ka: ...):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:  # (double-checked locking)
                    cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(self, frame: "types.FrameType | None" = None):
        if hasattr(self, "_server"):  # already init, bail out
            return

        self._server = socket.socket()
        try:
            self._server.bind(("localhost", 4444))
            self._server.listen(1)
        except Exception as e:
            self._server.close()
            raise

        print("Accepting connection...")
        self._conn, self._addr = self._server.accept()
        self._io = self._conn.makefile("rw")
        print("Got address", self._addr)
        self._print("Hello", self._addr)

        if frame is not None:
            self._trace = [frame]
            for back in iter(lambda: self._trace[-1].f_back, None):
                self._trace.append(back)
            self._curr = 0
            self._globals = self._trace[0].f_globals
            self._locals = self._trace[0].f_locals
        else:
            self._print("Could not get a stack frame :<")
            self._trace = []
            self._curr = -1
            self._globals = globals()
            self._locals = locals()

        self._state = trrepl.States.READLINE
        self._loop()

    def __del__(self):
        self.close()

    def _line(self):
        if self._io.closed:
            return ""
        self._io.write(self._state.value)
        self._io.flush()
        try:
            return self._io.readline()
        except KeyboardInterrupt:
            return ".\n"

    def _print(self, *a: ..., **ka: ...):
        if self._io.closed:
            return
        print(*a, **ka, file=self._io)

    def _assess_statement(self, line: str):
        beg = line.lstrip()
        eq = line.find("=")
        return any(
            beg.startswith(kw)
            for kw in [
                "case",
                "class",
                "def",
                "for",
                "global",
                "if",
                "import",
                "try",
                "while",
                "with",
            ]
        ) or (
            0 < eq
            and line[eq + 1] != "="
            and line[eq - 1] != "="
            and not "(" in line[:eq]
        )

    def _assess_continues(self, line: str):
        return (
            1 < len(line)  # "" and "\n" don't continue
            if trrepl.States.CONTINUE == self._state
            else line.rstrip()[-1] in "(:[{" or '"""' in line
        )

    def _loop(self):
        accu: "list[str]" = []
        for line in iter(self._line, ""):
            if trrepl.States.DONE == self._state:
                break

            if "." == line.strip():
                accu.clear()
                self._state = trrepl.States.READLINE
                continue

            if "#" == line.lstrip()[:1]:
                continue
            accu.append(line)

            if self._assess_continues(line):
                self._state = trrepl.States.CONTINUE
                continue

            self._globals["__"] = self
            a = "".join(accu), self._globals, self._locals
            try:
                r = exec(*a) if self._assess_statement(accu[0]) else eval(*a)
                if r is not None:
                    pprint.pprint(r, stream=self._io)
                    globals()["_"] = r
            except Exception as e:
                self._print(e)
            self._globals.pop("__")

            accu.clear()
            self._state = trrepl.States.READLINE

        self.close()

    def backtrace(self):
        for k, frame in enumerate(self._trace):
            self._print(
                "[curr]" if self._curr == k else "      ",
                f"{frame.f_code.co_filename}:{frame.f_lineno} "
                + f"in {frame.f_code.co_name}",
            )

    def frame(self, n: int = 0):
        frame = self._trace[n]
        self._print(
            f"[curr] {frame.f_code.co_filename}:{frame.f_lineno} "
            + f"in {frame.f_code.co_name}"
        )
        locs = (k for k in frame.f_locals.keys() if "_" != k[0])
        self._print(*locs, sep="\t\n")

        self._curr = n
        self._globals = frame.f_globals
        self._locals = frame.f_locals

    def grab_stdio(self):
        self._pstdio = sys.stdin, sys.stdin, sys.stdin
        sys.stdin, sys.stdin, sys.stdin = (self._io,) * 3

    def release_stdio(self):
        sys.stdin, sys.stdin, sys.stdin = self._pstdio
        del self._pstdio

    def close(self):
        if trrepl.States.DONE == self._state:  # eg. instance getting gc'd
            return

        self._print("Goodby", self._addr)
        print("Closing with", self._addr)
        self._io.flush()

        if hasattr(self, "_pstdio"):
            self.release_stdio()

        if not self._io.closed:
            self._io.close()
        self._conn.close()
        self._server.close()

        with trrepl._lock:
            trrepl._instance = None

        self._state = trrepl.States.DONE


def initrrepl(save_pid_to_file: str | None = None):
    if save_pid_to_file:
        with open(save_pid_to_file, "w") as pid:
            print(os.getpid(), file=pid)

    signal.signal(
        signal.SIGUSR1,
        lambda _, f: threading.Thread(target=trrepl, args=(f,)).start(),
    )
