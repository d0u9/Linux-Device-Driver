#! /bin/sh
module="short"
device="short"
mode="666"
group=0

function load() {
    insmod ./$module.ko $* || exit 1

    rm -f /dev/${device}[0-2]

    major=$(awk -v device="$device" '$2==device {print $1}' /proc/devices)
    mknod /dev/${device}0 c $major 0
    mknod /dev/${device}1 c $major 1
    mknod /dev/${device}2 c $major 2
    mknod /dev/${device}3 c $major 3
    mknod /dev/${device}4 c $major 4
    mknod /dev/${device}5 c $major 5
    mknod /dev/${device}6 c $major 6
    mknod /dev/${device}7 c $major 7

    mknod /dev/${device}16 c $major 16
    chgrp $group /dev/${device}16
    chmod $mode /dev/${device}16

    chgrp $group /dev/$device[0-7]
    chmod $mode /dev/$device[0-7]
}

function unload() {
    rm -f /dev/${device}[0-7]
    rm -f /dev/${device}16
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

