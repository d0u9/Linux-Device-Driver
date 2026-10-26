#!/bin/sh
# The USB core creates /dev/skel0, ... itself when the device is bound.
module="usb_skel"

load() {
    insmod ./$module.ko "$@" || exit 1
}

unload() {
    rmmod $module || exit 1
}

arg=${1:-"load"}
case $arg in
    load)
        load ;;
    unload)
        unload ;;
    reload)
        ( unload )
        load
        ;;
    *)
        echo "Usage: $0 {load | unload | reload}"
        echo "Default is load"
        exit 1
        ;;
esac
