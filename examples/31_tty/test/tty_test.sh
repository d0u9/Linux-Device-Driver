#!/bin/sh
# Run in the guest with the module loaded: read the timer's characters,
# write lines to printk, and change the line settings on /dev/lddtty*.
fails=0

check() {
    if [ "$1" -eq 0 ]; then
        echo "ok   $2"
    else
        echo "FAIL $2"
        fails=$((fails + 1))
    fi
}

n=0
for i in 0 1 2 3; do [ -c /dev/lddtty$i ] && n=$((n + 1)); done
[ $n -eq 4 ]
check $? "/dev/lddtty0..3 created"

# raw mode: no line buffering, so each 'z' is read as it arrives
got=$( (stty raw -echo; dd bs=1 count=2 2>/dev/null) < /dev/lddtty0)
[ "$got" = "zz" ]
check $? "two characters read from lddtty0"

open=$( (sleep 1; cat /proc/tty/driver/lddtty) < /dev/lddtty1)
echo "$open" | grep -qx 1
check $? "lddtty1 listed as open in /proc"
cat /proc/tty/driver/lddtty | grep -qx 1
[ $? -ne 0 ]
check $? "lddtty1 no longer listed after close"

echo "hello tty" > /dev/lddtty2
dmesg | grep -q '\[lddtty2\] hello tty$'
check $? "line written to lddtty2 printed"

long=$(printf '%0100d' 0)
echo "$long" > /dev/lddtty2
dmesg | tail -n 2 | grep -q '\\$'
check $? "a 100-char line split after 80"

speed=$( (stty 19200; stty speed) < /dev/lddtty3)
[ "$speed" = "19200" ]
check $? "baud rate set on lddtty3"

if [ $fails -eq 0 ]; then echo PASSED; else echo FAILED; fi
[ $fails -eq 0 ]
