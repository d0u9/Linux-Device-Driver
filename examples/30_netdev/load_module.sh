#! /bin/sh
module="netdev"
device="netdev"
mode="666"
group=0

function load() {
    insmod ./$module.ko use_napi=1 || exit 1
    ip link set dev sn0 up
    ip addr add dev sn0 172.16.100.0/24
    ip link set dev sn1 up
    ip addr add dev sn1 172.16.101.1/24
}

function unload() {
    ip link set dev sn0 down
    ip link set dev sn1 down
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

