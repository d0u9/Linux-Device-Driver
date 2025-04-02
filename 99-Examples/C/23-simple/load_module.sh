#! /bin/sh
module="simple"
device="simple"
mode="666"
group=0

function load() {
    insmod ./$module.ko $* || exit 1

    rm -f /dev/${device}[0-1]

    major=$(awk -v device="$device" '$2==device {print $1}' /proc/devices)
    mknod /dev/${device}0 c $major 0
    mknod /dev/${device}1 c $major 1

    chgrp $group /dev/${device}[0-1]
    chmod $mode /dev/${device}[0-1]
}

function unload() {
    rm -f /dev/${device}[0-1]
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

