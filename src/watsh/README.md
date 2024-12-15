Watch files for changes.

### Getting Started

```console
$ ./build # or just cc *.c -o watsh
$ ./watsh --help
Usage: ./watsh [opts] [--] <files>

  --help    -h         display this help
  --version            show the build version
  //--full    -f         output full paths
```
<!-- side note: this --help message is used in building the actual application -->

```console
$ ./watsh *.[ch] | while read; do ./build; done
```

### Usage

`watsh files...` will watches for operation on the
specified files. When a batch of changes is observed,
the paths of the objects in question are printed on
a single line as a PATH-like `:`-separated list.
I'm done, this was not interesting
