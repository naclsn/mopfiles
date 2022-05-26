#!/usr/bin/env sh

c=:
b=${1:-uh.asm}
a=${b%.asm}
if [ '-r' = "$b" ]
    then
        a=uh
    else
        if [ '-h' = "$b" ] || [ "$a" = "$b" ] || ! [ -f "$b" ]
            then
                echo "Usage: $0 [<file.asm>] [-r <args>]"
                echo '  default file: ./uh.asm'
                exit 2
        fi
        [ 0 -eq $# ] || shift
fi
if [ '-r' = "$1" ]
    then
        shift
        c="./bin/$a"
fi

set -ex
nasm -f elf64 "$a.asm"
ld -s -o "bin/$a" "$a.o"
rm "$a.o"

"$c" $@
{ set +x; } 2>/dev/null
echo + "# exit code: $?"
