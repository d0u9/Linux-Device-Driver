#!/usr/bin/env bash
set -eu

if [ "$#" -ne 1 ] || [ -z "$1" ]; then
    printf 'Usage: %s /path/to/ldd_root\n' "$0" >&2
    exit 2
fi

TOOLS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
mkdir -p -- "$1"
RUNTIME_DIR="$(cd -- "$1" && pwd -P)"
if [ "$RUNTIME_DIR" = "$TOOLS_DIR" ] || [ "$RUNTIME_DIR" = "$(dirname -- "$TOOLS_DIR")" ]; then
    printf 'Choose a separate runtime directory, not the tutorial checkout or tools directory.\n' >&2
    exit 2
fi

mkdir -p -- "$RUNTIME_DIR"/{bin,src,kernels,initramfs,shared}
install -m 644 "$TOOLS_DIR/activate" "$RUNTIME_DIR/activate"
install -m 644 "$TOOLS_DIR/clangd" "$RUNTIME_DIR/.clangd"
install -m 755 "$TOOLS_DIR/bin/qemu" "$TOOLS_DIR/bin/boot.sh" \
    "$TOOLS_DIR/bin/rebuild_initramfs.sh" "$RUNTIME_DIR/bin/"

printf 'Created workspace directories and installed activate, .clangd, qemu, boot.sh and rebuild_initramfs.sh in %s\n' "$RUNTIME_DIR"
printf 'Add this line to ~/.bashrc or ~/.zshrc, then open a new shell:\nsource %q\n' "$RUNTIME_DIR/activate"
