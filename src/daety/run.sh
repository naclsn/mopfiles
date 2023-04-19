#!/bin/sh -e
does() { echo "$@"; "$@"; }
for f in *.[ch]
  do [ $f -nt daety ] && does cc main.c client.c server.c -o daety -Wall -Wextra -pedantic && break
done
[ $1- = -n- ] || does exec ./daety "$@"
