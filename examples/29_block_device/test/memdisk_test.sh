#!/bin/sh
# Run in the guest with the module loaded: check size, data, the end of the
# disk, partitions and a file system on /dev/memdisk.
dev=/dev/memdisk
params=/sys/module/memdisk/parameters
fails=0

check() {
    if [ "$1" -eq 0 ]; then
        echo "ok   $2"
    else
        echo "FAIL $2"
        fails=$((fails + 1))
    fi
}

bs=$(cat $params/hardsect_size)
size=$(( $(cat $params/nsectors) * bs ))

[ "$(blockdev --getsize64 $dev)" = "$size" ]
check $? "size is $size bytes"
[ "$(blockdev --getss $dev)" = "$bs" ]
check $? "block size is $bs bytes"

# 64 KB of random data at 100 KB, read back and compared
dd if=/dev/urandom of=/tmp/pattern bs=1024 count=64 2>/dev/null
dd if=/tmp/pattern of=$dev bs=1024 seek=100 conv=fsync 2>/dev/null
echo 3 > /proc/sys/vm/drop_caches
dd if=$dev of=/tmp/back bs=1024 skip=100 count=64 2>/dev/null
cmp -s /tmp/pattern /tmp/back
check $? "data written and read back"

dd if=/dev/zero of=$dev bs=$bs seek=$((size / bs)) count=1 2>/dev/null
[ $? -ne 0 ]
check $? "write past the end refused"

# one partition over the whole disk, with an ext2 file system on it;
# busybox fdisk only handles 512-byte sectors, so otherwise use the disk
part=$dev
if [ "$bs" -eq 512 ]; then
    printf 'o\nn\np\n1\n\n\nw\n' | fdisk $dev >/dev/null 2>&1
    [ -b ${dev}1 ]
    check $? "partition ${dev}1 created"
    part=${dev}1
fi
mke2fs -q -b $([ $bs -gt 1024 ] && echo $bs || echo 1024) $part >/dev/null 2>&1
check $? "ext2 made on $part"
mkdir -p /mnt/memdisk
mount -t ext2 $part /mnt/memdisk && echo hello > /mnt/memdisk/file &&
    umount /mnt/memdisk && mount -t ext2 $part /mnt/memdisk
[ "$(cat /mnt/memdisk/file 2>/dev/null)" = "hello" ]
check $? "file survives umount and mount"
umount /mnt/memdisk 2>/dev/null

if [ $fails -eq 0 ]; then echo PASSED; else echo FAILED; fi
[ $fails -eq 0 ]
