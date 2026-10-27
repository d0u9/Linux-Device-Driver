#!/bin/sh
# Extra arguments go to insmod, e.g. "sh load_module.sh load use_napi=1".
# sn0 is 192.168.0.1 and reaches sn1 as 192.168.0.2; sn1 is 192.168.1.2
# and reaches sn0 as 192.168.1.1.
module="netdev"

load() {
    insmod ./$module.ko "$@" || exit 1
    ip addr add 192.168.0.1/24 dev sn0
    ip addr add 192.168.1.2/24 dev sn1
    ip link set sn0 up
    ip link set sn1 up
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
