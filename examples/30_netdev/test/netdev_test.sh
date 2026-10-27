#!/bin/sh
# Run in the guest after "sh load_module.sh": ping and TCP between sn0 and
# sn1, which only reach each other through the driver.
fails=0

check() {
    if [ "$1" -eq 0 ]; then
        echo "ok   $2"
    else
        echo "FAIL $2"
        fails=$((fails + 1))
    fi
}

stat() {
    cat /sys/class/net/$1/statistics/$2
}

rx0=$(stat sn0 rx_packets)
rx1=$(stat sn1 rx_packets)

ping -c 3 -W 1 -I sn0 192.168.0.2 >/dev/null 2>&1
check $? "ping 192.168.0.2 from sn0, answered by sn1"
ping -c 3 -W 1 -I sn1 192.168.1.1 >/dev/null 2>&1
check $? "ping 192.168.1.1 from sn1, answered by sn0"
ping -c 2 -W 1 -s 1472 -I sn0 192.168.0.2 >/dev/null 2>&1
check $? "full-size frames (1514 bytes)"

[ $(stat sn0 rx_packets) -gt $rx0 ] && [ $(stat sn1 rx_packets) -gt $rx1 ]
check $? "rx_packets counted on both devices"

# 256 KB over TCP: a server on sn1's side, a client on sn0's
dd if=/dev/urandom of=/tmp/sent bs=1024 count=256 2>/dev/null
nc -l -p 5001 > /tmp/got &
sleep 1
nc -w 3 192.168.0.2 5001 < /tmp/sent
wait
cmp -s /tmp/sent /tmp/got
check $? "256 KB over TCP arrive intact"

if [ $fails -eq 0 ]; then echo PASSED; else echo FAILED; fi
[ $fails -eq 0 ]
