#!/bin/sh
n=${1%.c}; shift
make $n && exec ./$n "$@"
