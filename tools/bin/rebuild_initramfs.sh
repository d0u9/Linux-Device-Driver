#!/bin/bash

set -e

: "${LDD_ROOT:?Enter the configured LDD workspace or export LDD_ROOT first.}"
cd "$LDD_ROOT/initramfs"

find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz

echo " * Rebuilt initramfs.cpio.gz"
ls -la ../initramfs.cpio.gz
