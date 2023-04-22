Make any terminal app into server/client. Basically
single-window-single-panel `tmux(1)`, but dumber.

### Getting Started

```console
$ ./build # or just cc *.c -o daety
$ ./daety --help
Usage: ./daety [opts] <prog> [<args...>]

--help    -h         display this help
--version            show build the version

shared client/server:
--id      -i <id>    the default is form prog and args
(TODO: socket options)

server only:
--server  -s         starts only the server
--verbose            (only when -s)

client only:
--key     -k <key>   use the following leader key
--cmd     -c <keys..> --
                     send the sequence upon connection
                     must be terminated with --
                     ^x are translated to controls
--cooked             do not set raw mode
```

```console
$ ./daety vi # in one terminal
... (vi)

$ ./daety -c :e README.md ^M -- vi # in another terminal
... (same vi)
```

### Usage

`daety prog args...` will start `prog` on a server if an
existing instance was not already started with the exact
same arguments. Then it connects a client to interract
with the program. If a server already existed, a client is
created and connected to it. Multiple client can connect
to a same server.

Options for `daety` must be position before `prog`. `--`
is recognised as ending the options.

`--id/-i` can be used to identify a server not by
the combination of arguments. A server can be started
explicitely with `--server/-s`. It will then not be
a daemon. IDs have a max length of 1019.

Client can be specified a 'leader key' with `--key/-k`.
The `^` notation is used (for example esc is `^[`, see
`showkey -a`). The default is `^\`. The leader key is used
to talk with the client itself:
 - `<key>^C`: terminate the server, close the client
 - `<key>^D`: close the client, keep the server
 - `<key>^Z`: pause the client (SIGTSTP) -- not implemented

By default, the client sets itself into raw mode, ie
each key press is immediately sent to the server. To
keep in cooked mode (typically when the program is not an
interactive TUI), `--cooked` can be used.

### Notes

Both the client and the server are perfectly dumb and
oblivious to transiting bytes. Nothing is stored. However,
the server scans the program's output to track entering /
leaving alternate buffer, and the client scans for the
leader key.
