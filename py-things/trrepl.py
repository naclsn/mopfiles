"""
Threaded Remote Read Eval Print Loop (trrepl - read "triple")

TODO: docstring, general use and such
"""


class trrepl:
    """
    TODO: docstring with use as help(__) in mind
    """

    from threading import Lock
    from types import FrameType
    from enum import Enum

    _instance = None
    _lock = Lock()

    class settings:
        """
        * port: port to use, default is 4099
        * logger: a logger to use (server-side), or None
        * pprint: whether to use pprint or simply repr
        * greeting: message printed once uppon successful connection
        """

        from logging import Logger, getLogger

        port: int = 4099
        logger: "Logger | None" = getLogger(__file__)
        pprint: bool = True
        greeting: str = (
            "__ is the trrepl itself; see help(__), or eg help(__.frame)\n"
            "help(__.settings) for the settings and their current values\n"
        )

        del Logger, getLogger

    class _States(Enum):
        READLINE = "<<< "
        CONTINUE = ",,, "
        DONE = "?!"

    def __new__(cls, *a: ..., **ka: ...):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:  # (double-checked locking)
                    cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(self, frame: "FrameType | None" = None):
        if hasattr(self, "_server"):  # already init, bail out
            return

        from socket import socket

        self._server = socket()
        try:
            self._server.bind(("localhost", trrepl.settings.port))
            self._server.listen(1)
        except BaseException as e:
            if trrepl.settings.logger:
                print("Could not bind socket:", e)
            self._server.close()
            raise

        if trrepl.settings.logger:
            print("Accepting connection...")
        try:
            self._conn, self._addr = self._server.accept()
        except BaseException as e:
            if trrepl.settings.logger:
                print("Could not accept connection:", e)
            self._server.close()
            raise

        self._io = self._conn.makefile("rw")
        if trrepl.settings.logger:
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

        import pydoc

        self._phelp = pydoc.help, pydoc.pager
        pydoc.help = pydoc.Helper(input=self._io, output=self._io)
        pydoc.pager = self._print
        self._print(trrepl.settings.greeting)

        self._state = trrepl._States.READLINE
        self._loop()

    def __del__(self):
        if hasattr(self, "_state"):  # ensure was fully init
            self.close()

    def _line(self):
        if self._io.closed:
            return ""
        try:
            self._io.write("\1" + self._state.value + "\2")
            self._io.flush()
            return self._io.readline()
        except KeyboardInterrupt:
            return ".\n"
        except ConnectionError:
            return ""

    def _print(self, *a: ..., **ka: ...):
        if self._io.closed:
            return
        try:
            print(*a, **ka, file=self._io)
        except ConnectionError:
            pass

    def _assess_statement(self, line: str):
        """answers: should we use `exec` instead of `eval`?"""
        beg = line.lstrip()
        eq = beg.find("=")
        return any(
            beg.startswith(kw)
            for kw in [
                "case",
                "class",
                "def",
                "del",
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
            and line[eq - 1] not in "!:<=>"
            and not sum(("(" == c) - (")" == c) for c in line[:eq])
        )

    def _assess_continues(self, line: str):
        """answers: should we ask for another line of input?"""
        return "\n" != line and (
            line.isspace() or line.rstrip()[-1] in "(:[{" or '"""' in line
        )

    def _loop(self):
        accu: "list[str]" = []
        for line in iter(self._line, ""):
            if "." == line.strip():  # just like ^C in the normal repl
                accu.clear()
                self._state = trrepl._States.READLINE
                continue

            if "#" == line.lstrip()[:1]:
                continue
            accu.append(line)

            if self._assess_continues(line):
                self._state = trrepl._States.CONTINUE
                continue

            source = "".join(accu)
            run = exec if self._assess_statement(accu[0]) else eval
            accu.clear()
            self._state = trrepl._States.READLINE

            if source.isspace():
                continue

            self._globals["__"] = self
            try:
                _ = run(source, self._globals, self._locals)
                if _ is not None:
                    if trrepl.settings.pprint:
                        from pprint import pprint

                        pprint(
                            _,
                            stream=self._io,
                            compact=True,
                            sort_dicts=False,
                            underscore_numbers=True,
                        )
                    else:
                        print(repr(_))
                    # TODO(maybe): globals()["_"] = _
            except BaseException as e:
                self._print(e)
            self._globals.pop("__")

        self.close()

    def backtrace(self):
        """shows the backtrace from top to bottom"""
        for k, frame in enumerate(self._trace):
            self._print(
                "[curr]" if self._curr == k else "      ",
                f"{frame.f_code.co_filename}:{frame.f_lineno} "
                + f"in {frame.f_code.co_name}",
            )

    def frame(self, n: int = 0):
        """set current frame to the (absolute) nth; 0 is top, -1 would be bottom"""
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
        """changes the sys stdin, out and err to go through the connection"""
        if not hasattr(self, "_pstdio"):
            import sys

            self._pstdio = sys.stdin, sys.stdout, sys.stderr
            sys.stdin, sys.stdout, sys.stderr = (self._io,) * 3

    def release_stdio(self):
        """resets the sys stdin, out and err to their original"""
        if hasattr(self, "_pstdio"):
            import sys

            sys.stdin, sys.stdout, sys.stderr = self._pstdio
            del self._pstdio

    def close(self):
        """closes the connection; equivalent to, well, closing the connection"""
        if trrepl._States.DONE == self._state:  # eg. instance getting gc'd
            return

        import pydoc

        pydoc.help, pydoc.pager = self._phelp
        self.release_stdio()

        self._print("Goodby", self._addr, flush=True)
        if trrepl.settings.logger:
            print("Closing with", self._addr)

        if not self._io.closed:
            try:
                self._io.close()
            except ConnectionError:
                pass
        try:
            self._conn.close()
            self._server.close()
        except BaseException:  # ... anything could go wrong, probly...
            raise

        finally:
            with trrepl._lock:
                trrepl._instance = None

            self._state = trrepl._States.DONE

    del Enum, FrameType, Lock


def initrrepl(
    *,
    save_pid_to_file: "str | None" = None,
    edit_settings: "trrepl.settings | None" = None,
):
    """
    Setup signal handling (SIGUSR1).

    If `save_pid_to_file` is given, saves the pid to the given file. Use
    `edit_settings` to change some of the values (see `trrepl.settings`).
    """
    from os import getpid
    from threading import Thread
    from signal import SIGUSR1, signal

    if save_pid_to_file:
        with open(save_pid_to_file, "w") as pid:
            print(getpid(), file=pid)

    if edit_settings:
        unchanged = object()
        for k in dir(edit_settings):
            setting = getattr(edit_settings, k, unchanged)
            if setting is not unchanged:
                setattr(trrepl.settings, k, setting)

    signal(SIGUSR1, lambda _, f: Thread(target=trrepl, args=(f,)).start())


if "__main__" == __name__:
    from argparse import ArgumentParser, ArgumentTypeError

    class uint(int):
        def __new__(cls, value: str):
            r = super().__new__(cls, value)
            if r <= 0:
                raise ArgumentTypeError(f"invalid positive int: '{value}'")
            return r

    def pid_or_pid_file(value: str):
        try:
            pid = uint(value)
        except ValueError:
            try:
                with open(value, "r") as file:
                    pid = uint(file.read().strip())
            except FileNotFoundError:
                raise ArgumentTypeError(f"file not found: '{value}'")
        return pid

    def address(value: str):
        host, _, port = value.rpartition(":")
        return host or "localhost", uint(port)

    parse = ArgumentParser(
        description=(
            "this is more or less equivalent to running:"
            " kill <pid> && rlwrap nc <w> <port>"
        ),
    )
    parse.add_argument(
        "-c",
        help="program passed in as string",
        metavar="cmd",
    )
    parse.add_argument(
        "-i",
        action="store_true",
        help=(
            "inspect interactively after running script; forces a prompt even"
            " if stdin does not appear to be a terminal"
        ),
    )
    parse.add_argument(
        "-w",
        default=1,
        help="connect timeout in seconds; defaults to 1s",
        metavar="sec",
        type=float,
    )
    parse.add_argument(
        "pid",
        metavar="pid_or_pid_file",
        type=pid_or_pid_file,
    )
    parse.add_argument(
        "address",
        default=("localhost", trrepl.settings.port),
        nargs="?",
        type=address,
    )

    opts = parse.parse_args()

    from os import kill
    from signal import SIGUSR1
    from time import sleep

    kill(opts.pid, SIGUSR1)
    sleep(1)  # :/

    from atexit import register
    from socket import socket

    client = socket()
    client.settimeout(opts.w)
    try:
        client.connect(opts.address)
    except ConnectionError:
        try:
            client.connect(opts.address)
        except ConnectionError:
            client.connect(opts.address)
    register(client.close)

    def read_to_prompt():
        buffer: "list[bytes]" = []
        text: "str | None" = None

        for chunk in iter(lambda: client.recv(1024), ""):
            chunk, found, follows = chunk.partition(b"\1" if text is None else b"\2")
            buffer.append(chunk)
            if not found:
                continue

            accu = b"".join(buffer).decode()
            buffer.clear()
            if text is None:
                text = accu
                prompt, found, follows = follows.partition(b"\2")
                if found:
                    yield text, prompt.decode()
                    text = None
                else:
                    follows = prompt
            else:
                yield text, accu
                text = None

            buffer.append(follows)

        left = (text or "") + b"".join(buffer).decode()
        if left:
            yield left, str()

    from sys import stdin

    text_prompt = read_to_prompt()
    text, prompt = next(text_prompt)
    if not opts.c and stdin.isatty():
        print(text, sep="", end="")

    if opts.c:
        client.sendall(opts.c.encode())
        text, prompt = next(text_prompt)
        print(text, end="")

    if not stdin.isatty():
        for line in stdin:
            client.sendall(line.encode())
            text, prompt = next(text_prompt)
            print(text, end="")

    if opts.i or not opts.c and stdin.isatty():
        if not stdin.isatty():
            import os

            tty_fd = os.open("/dev/tty", os.O_RDWR)
            os.dup2(tty_fd, stdin.fileno())
            os.close(tty_fd)

        try:
            import readline as _

            # TODO(maybe): history? completion? etc...
        except:
            pass

        while 1:
            try:
                line = input(prompt)
            except KeyboardInterrupt:
                line = "."
            except EOFError:
                print()
                break
            client.sendall(line.encode() + b"\n")
            text, prompt = next(text_prompt)
            print(text, end="")
