#!/bin/bash

set -e

SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
export LDD_ROOT="$(cd "${SCRIPTDIR}"/.. && pwd)"
cd "$LDD_ROOT/initramfs"

find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz

echo " * Rebuilt initramfs.cpio.gz"
ls -la ../initramfs.cpio.gz
