"""Threaded Remote Read Eval Print Loop (trrepl - read "triple")

License: MIT License - Copyright (c) 2024 Grenier Celestin
Just, know what you're using and doing-

Provides an interactive REPL, somewhat like the Python REPL, on a running
program without interrupting it. The REPL will be available through a socket.
This does not add any overhead outside of active use (the necessary resources
are only allocated when an interactive session is live).

    from trrepl import initrrepl
    initrrepl(save_pid_to_file="/tmp/my.pid")

Once setup and the program running, it will react to SIGUSR1 by opening
a server socket on (configurable) port 4099 in its own thread. This very module
can be used as a client (with readline(3) if available):

    $ python -m trrepl /tmp/my.pid

This is equivalent to:

    $ kill -SIGUSR1 `cat /tmp/my.pid`
    $ nc localhost 4099

Limitations:
* uses SIGUSR1, so it cannot be overloaded without potentially conflicting
* uses a port (4099 by default), so it needs to be available when needed

TODO/FIXME:
* accessing other thread, also async stuff (could be cool)
* locals situation, it needs to behave as expectable
* enable more socket types (eg. unix-domain)
"""

import sys


# API {{{1
__all__ = ["acceptrrepl", "initrrepl", "trrepl"]


from types import FrameType


def acceptrrepl(
    frame: "FrameType | None" = None,
    settings: "trrepl.Settings | None" = None,
):
    """Accept incoming connection now.

    If the frame is not specified, latch onto the last one akin to
    `pdb.set_trace`. This is of course non blocking.
    """
    from threading import Thread

    if not settings:
        settings = trrepl.Settings()

    args = trrepl.Settings(**vars(settings)), frame or sys._getframe().f_back
    Thread(target=trrepl, args=args).start()


del FrameType


def initrrepl(
    save_pid_to_file: "str | None" = None,
    settings: "trrepl.Settings | None" = None,
):
    """Setup signal handling (SIGUSR1).

    If `save_pid_to_file` is given, saves the pid to the given file. Use
    `settings` to change some of the initial values (see `trrepl.Settings`).
    """
    from os import getpid
    from signal import SIGUSR1, signal

    if save_pid_to_file:
        with open(save_pid_to_file, "w") as pid:
            print(getpid(), file=pid)

    signal(SIGUSR1, lambda _, frame: acceptrrepl(frame, settings))


class trrepl:
    """TODO: docstring with use as help(__) in mind"""

    from dataclasses import dataclass
    from enum import Enum
    from re import compile
    from types import FrameType

    # lifecycle {{{2
    @dataclass
    class Settings:
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

    def __init__(self, settings: Settings, frame: "FrameType | None" = None):
        self.settings = trrepl.Settings()
        vars(self.settings).update(vars(settings))

        from socket import socket

        self._server = socket()
        try:
            self._server.bind(("localhost", self.settings.port))
            self._server.listen(1)
        except BaseException as e:
            self._log("error", "Could not bind socket:", e)
            self._server.close()
            raise

        self._log("info", "Accepting connection...")
        try:
            self._conn, self._addr = self._server.accept()
        except BaseException as e:
            self._log("error", "Could not accept connection:", e)
            self._server.close()
            raise

        self._io = self._conn.makefile("rw")
        self._log("info", "Got address", self._addr)
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
        self._print(self.settings.greeting)

        self._state = trrepl._States.READLINE
        self._loop()

    def __del__(self):
        if hasattr(self, "_state"):  # ensure was fully init
            self.close()

    # io {{{2
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

    def _log(self, level: str, *a: ..., **ka: ...):
        if self.settings.logger:
            getattr(self.settings.logger, level)(*a, **ka)

    def displayhook(self, v: object):
        del self._globals["_"]
        if self.settings.pprint:
            from pprint import pprint

            pprint(
                v,
                stream=self._io,
                compact=True,
                sort_dicts=False,
                underscore_numbers=True,
            )
        else:
            self._print(repr(v))
        self._globals["_"] = v

    def excepthook(self, _ty: type, e: BaseException, _trace: ...):
        self._print(e)

    # loop {{{2
    class _States(Enum):
        READLINE = "<<< "
        CONTINUE = ",,, "
        CLOSED = "(done)"

    def _loop(self):
        from codeop import compile_command
        from types import FunctionType

        source = ""
        for line in iter(self._line, ""):
            source += line
            if "." == line.strip():  # bit like ^C in the normal repl
                source = ""
                self._state = trrepl._States.READLINE
                continue

            try:
                co = compile_command(source)
                if co is None:  # command is incomplete
                    self._state = trrepl._States.CONTINUE
                    continue

                self._globals["__"] = self
                # TODO: maybe have a way that this times out if requested
                #       (eg inf. loop, as there is no way to ^C)
                self.displayhook(FunctionType(co, self._globals)())
                self._globals.pop("__")

            except BaseException as e:
                self.excepthook(type(e), e, None)

            source = ""
            self._state = trrepl._States.READLINE

        self.close()

    # 'commands' {{{2
    def backtrace(self):
        """shows the backtrace from top to bottom"""
        for k, frame in enumerate(self._trace):
            self._print(
                "[curr]" if self._curr == k else "      ",
                f"{frame.f_code.co_filename}:{frame.f_lineno} "
                + f"in {frame.f_code.co_name}",
            )

    def frame(self, n: int = 0):
        """set current frame to (absolute) nth; 0 is top, -1 would be bottom"""
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

    def frames(self):
        """see sys._current_frames, returns that"""
        return sys._current_frames()

    def grab_stdio(self):
        """changes the sys stdin, out and err to go through the connection

        this makes it possible to use eg. `__._displayhook = sys.displayhook`
        (restore with `del __._displayhook`)
        """
        if not hasattr(self, "_pstdio"):
            self._pstdio = sys.stdin, sys.stdout, sys.stderr
            sys.stdin, sys.stdout, sys.stderr = (self._io,) * 3

    def release_stdio(self):
        """resets the sys stdin, out and err to their original"""
        if hasattr(self, "_pstdio"):
            sys.stdin, sys.stdout, sys.stderr = self._pstdio
            del self._pstdio

    def close(self):
        """closes the connection, same as just closing `nc`"""
        if trrepl._States.CLOSED == self._state:  # eg. instance getting gc'd
            return

        import pydoc

        pydoc.help, pydoc.pager = self._phelp
        self.release_stdio()

        self._print("Goodby", self._addr, flush=True)
        self._log("info", "Closing with", self._addr)

        if not self._io.closed:
            try:
                self._io.close()
            except ConnectionError:
                pass
        try:
            self._conn.close()
            self._server.close()

        finally:
            self._state = trrepl._States.CLOSED

    del Enum, FrameType, compile


# CLI {{{1
if "__main__" == __name__:
    # args {{{2
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
            " kill <pid> && rlwrap nc <-w> localhost <port>"
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
        help="connect timeout in seconds; default: 1s",
        metavar="sec",
        type=float,
    )
    parse.add_argument(
        "pid",
        help="if not a pid, should be a file with a pid in it",
        metavar="pid_or_pid_file",
        type=pid_or_pid_file,
    )
    parse.add_argument(
        "address",
        default=("localhost", trrepl.Settings.port),
        help=f"in the form [<host>:]<port>; default: {trrepl.Settings.port}",
        nargs="?",
        type=address,
    )

    opts = parse.parse_args()

    # prep {{{2
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
            chunk, found, t = chunk.partition([b"\2", b"\1"][text is None])
            buffer.append(chunk)
            if not found:
                continue

            accu = b"".join(buffer).decode()
            buffer.clear()
            if text is None:
                text = accu
                prompt, found, t = t.partition(b"\2")
                if found:
                    yield text, prompt.decode()
                    text = None
                else:
                    t = prompt
            else:
                yield text, accu
                text = None

            buffer.append(t)

        left = (text or "") + b"".join(buffer).decode()
        if left:
            yield left, str()

    # act {{{2
    text_prompt = read_to_prompt()
    text, prompt = next(text_prompt)
    istty = sys.stdin.isatty()
    if not opts.c and istty:
        print(text, sep="", end="")

    if opts.c:
        client.sendall(opts.c.encode())
        text, prompt = next(text_prompt)
        print(text, end="")

    if not istty:
        for line in sys.stdin:
            client.sendall(line.encode())
            text, prompt = next(text_prompt)
            print(text, end="")

    if opts.i or not opts.c and istty:
        if not istty:
            import os

            tty_fd = os.open("/dev/tty", os.O_RDWR)
            os.dup2(tty_fd, sys.stdin.fileno())
            os.close(tty_fd)

        try:
            import readline as _

            # TODO(maybe): history? completion? etc...
        except ModuleNotFoundError:
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
