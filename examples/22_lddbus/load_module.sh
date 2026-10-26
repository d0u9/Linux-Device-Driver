#!/bin/sh
# Load the bus first: sculld uses its symbols. Unload in reverse.
# lddbus.ko and sculld.ko are expected in the current directory.

load() {
    insmod ./lddbus.ko || exit 1
    insmod ./sculld.ko "$@" || { rmmod lddbus; exit 1; }
}

unload() {
    rmmod sculld
    rmmod lddbus || exit 1
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
