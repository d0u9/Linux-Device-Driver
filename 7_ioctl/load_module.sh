#!/bin/sh
module="ioctl"
device="ioctl-test"
mode="666"

cat /proc/devices | grep -q $device && /sbin/rmmod $module && echo "Removed module"
/sbin/insmod $module.ko || exit 1
echo "Module loaded successfully"

rm -f /dev/$device

major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)

mknod /dev/$device c $major 0

group="staff"
grep -q '^staff:' /etc/group || group=="wheel"

chgrp $group /dev/$device
chmod $mode /dev/$device

echo "Finished successfully!"
