#!/usr/bin/env sh

d=$([ '-d' = "$1" ] && echo 1); [ '-d' = "$1" ] && shift
c=:
b=${1:-uh.asm}
a=${b%.asm}
if [ '-r' = "$b" ]
    then
        a=uh
    else
        if [ '-h' = "$b" ] || [ "$a" = "$b" ] || ! [ -f "$b" ]
            then
                echo "Usage: $0 [-d] [<file.asm>] [-r <args>]"
                echo '  default file: ./uh.asm'
                echo '  use -d to build with debug symbol'
                exit 2
        fi
        [ 0 -eq $# ] || shift
fi
if [ '-r' = "$1" ]
    then
        shift
        c="./bin/$a"
fi
[ -d bin/ ] || mkdir bin/

set -ex
if [ -z "$d" ]
    then
        # release build
        nasm -f elf64 -o "bin/$a.o" "$a.asm"
        ld -m elf_x86_64 -s -o "bin/$a" "bin/$a.o"

        { set +e; } 2>/dev/null
        "$c" $@
        { echo + "# exit code: $?"; } 2>/dev/null

    else
        # debug build
        nasm -f elf64 -F dwarf -g -l "bin/$a.lst" -o "bin/$a.o" "$a.asm"
        ld -m elf_x86_64 -o "bin/$a" "bin/$a.o"

        gdb --args "$c" $@
fi
