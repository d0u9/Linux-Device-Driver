#! /bin/sh
module="memdisk"
device="memdisk"
mode="666"
group=0

function load() {
    insmod ./$module.ko $* request_mode=2 || exit 1

    rm -f /dev/${device}

    major=$(awk -v device="$device" '$2==device {print $1}' /proc/devices)
    mknod /dev/${device} b $major 0

    chgrp $group /dev/${device}
    chmod $mode /dev/${device}
}

function unload() {
    rm -f /dev/${device}
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

