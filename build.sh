#!/usr/bin/env sh

c=$1; [ 0 -eq $# ] || shift
set -ex
case $c in
    -r)
        make release
        ./bin/uh $@
        { echo + "# exit code: $?"; } 2>/dev/null
        ;;
    -d)
        make debug
        gdb -q --args ./bin/uh $@
        ;;
    *)
        set +x
        echo "Usage: $0 [-rd <args>]"
        echo '  use -r to run right away'
        echo '  use -d to build with debug symbol'
        exit 2
        ;;
esac
