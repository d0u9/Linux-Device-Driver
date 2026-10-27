#!/bin/sh
# The tty core creates /dev/lddtty0..3 itself, with a dynamic major.
module="lddtty"

load() {
    insmod ./$module.ko "$@" || exit 1
}

unload() {
    rmmod $module || exit 1
}

arg=${1:-"load"}
[ $# -gt 0 ] && shift
case $arg in
    load)
        load "$@" ;;
    unload)
        unload ;;
    reload)
        ( unload )
        load "$@"
        ;;
    *)
        echo "Usage: $0 {load | unload | reload} [module parameters]"
        echo "Default is load"
        exit 1
        ;;
esac
