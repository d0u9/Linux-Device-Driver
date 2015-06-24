#!/usr/bin/env sh
echo "Start loading..."

module="pipe"
device="pipe"
mode="666"

cat /proc/devices | grep -q "pipe" && rmmod pipe

rm -f /dev/${module}[0-2]

insmod ./pipe.ko || (echo "module load error!"; exit;)

echo "Module loaded!"

major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)

echo $major

mknod /dev/${device}0 c $major 0
mknod /dev/${device}1 c $major 1
mknod /dev/${device}2 c $major 2

echo "File created!"

group="staff"
grep -q '^staff:' /etc/group || group=="wheel"

chgrp $group /dev/$device[0-2]
chmod $mode /dev/$device[0-2]
