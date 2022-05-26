#!/usr/bin/env sh

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

nasm -f elf64 "$a.asm" && ld -s -o "bin/$a" "$a.o" && rm "$a.o" || exit 1

if [ '-r' = "$1" ]
    then
        shift
        echo "+ ./bin/$a $@"
        "./bin/$a" $@
        echo "+ finished with code $?"
fi
