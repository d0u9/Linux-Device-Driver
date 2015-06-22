#!/bin/sh
module="pipe"
device="pipe"
mode="666"

echo "Starting..."

cat /proc/devices | grep -q "pipe" && rmmod pipe
insmod ./pipe.ko

rm -f /dev/${device}[0-2]

major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices) || exit 1
echo "Major = "$major

mknod /dev/${device}0 c $major 0
mknod /dev/${device}1 c $major 1
mknod /dev/${device}2 c $major 2

group="staff"
grep -q '^staff:' /etc/group || group=="wheel"

chgrp $group /dev/$device[0-2]
chmod $mode /dev/$device[0-2]

