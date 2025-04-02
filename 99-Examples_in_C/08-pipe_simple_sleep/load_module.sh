#! /bin/sh
module="pipe_simple_sleep"
device="pipe_simple_sleep"
mode="666"
group=0

function load() {
    insmod ./$module.ko $* || exit 1

    rm -f /dev/${device}[0-3]

    major=$(awk -v device="$device" '$2==device {print $1}' /proc/devices)
    mknod /dev/${device}0 c $major 0
    mknod /dev/${device}1 c $major 1
    mknod /dev/${device}2 c $major 2
    mknod /dev/${device}3 c $major 3

    chgrp $group /dev/$device[0-3]
    chmod $mode /dev/$device[0-3]
}

function unload() {
    rm -f /dev/${device}[0-3]
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

