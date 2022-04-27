#!/usr/bin/env sh

a=${1%.asm}
if [ -z $1 ] || [ '-h' = "$1" ] || [ "$a" = "$1" ] || ! [ -f "$1" ]
    then
        echo "Usage: $0 <file.asm> [-r <args>]"
        exit 2
fi

nasm -f elf64 $a.asm && ld -s -o bin/$a $a.o && rm $a.o || exit 1

if [ '-r' = "$2" ]
    then
        shift 2
        echo "+ ./bin/$a $@"
        ./bin/$a $@
        echo "+ finished with code $?"
fi

