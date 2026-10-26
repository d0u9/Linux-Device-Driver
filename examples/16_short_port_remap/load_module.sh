#!/bin/sh
module="short_port_remap"
device="short_port_remap"
mode="666"
group=0

load() {
    insmod ./$module.ko "$@" || exit 1

    rm -f /dev/${device}_data /dev/${device}_matrix

    major=$(awk -v device="$device" '$2==device {print $1}' /proc/devices)
    mknod /dev/${device}_data c $major 0
    mknod /dev/${device}_matrix c $major 1

    chgrp $group /dev/${device}_data /dev/${device}_matrix
    chmod $mode /dev/${device}_data /dev/${device}_matrix
}

unload() {
    rm -f /dev/${device}_data /dev/${device}_matrix
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
