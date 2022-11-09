#!/bin/sh -ex
prog=$(basename $(dirname $(realpath "$0")))
cc main.c -o $prog -Wall -Wextra -pedantic
[ -f me ] && rm me
exec ./$prog "$@"
