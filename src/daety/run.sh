#!/bin/sh -e
does() { echo "$@"; "$@"; }
[ main.c -nt daety ] && does cc main.c -o daety -Wall -Wextra -pedantic
[ $1- = -n- ] || does exec ./daety "$@"
