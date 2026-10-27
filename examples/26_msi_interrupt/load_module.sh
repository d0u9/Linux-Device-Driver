#!/bin/sh
# Extra arguments go to insmod, e.g. "sh load_module.sh load msi=0".
module="msi_interrupt"

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
