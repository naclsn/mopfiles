#!/bin/sh -e
does() { echo "$@"; "$@"; }
prog=$(basename $(dirname $(realpath "$0")))
[ main.c -nt $prog ] && does cc main.c -o $prog -Wall -Wextra -pedantic
[ $1- = -n- ] || does exec ./$prog "$@"
