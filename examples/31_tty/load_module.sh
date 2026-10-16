#! /bin/sh
module="lddtty"
device="lddtty"
mode="666"
group=0

function load() {
    insmod ./$module.ko || exit 1

    rm -f /dev/${device}*

    major=$(awk -v device="$device" '$2==device {print $1}' /proc/devices)
    mknod /dev/${device}0 c $major 0

    chgrp $group /dev/${device}0
    chmod $mode /dev/${device}0
}

function unload() {
    rm -f /dev/${device}*
    rmmod $module
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

