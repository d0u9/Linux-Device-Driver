#!/usr/bin/env bash
set -eu

usage() {
    printf 'Usage: %s [--remove] [/path/to/qemu]\n' "$0" >&2
    exit 2
}

REMOVE=0
if [ "${1:-}" = --remove ]; then
    REMOVE=1
    shift
fi
[ "$#" -le 1 ] || usage

if [ "$#" -eq 1 ]; then
    QEMU_SRC="$1"
else
    : "${LDD_ROOT:?Enter the configured LDD workspace, export LDD_ROOT or pass the QEMU source path.}"
    QEMU_SRC="$LDD_ROOT/src/qemu"
fi

DEVICES_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/hw/ldd"
MESON="$QEMU_SRC/hw/meson.build"
KCONFIG="$QEMU_SRC/hw/Kconfig"
LINK="$QEMU_SRC/hw/ldd"
MESON_LINE="subdir('ldd')"
KCONFIG_LINE="source ldd/Kconfig"

if [ ! -f "$MESON" ] || [ ! -f "$KCONFIG" ]; then
    printf 'Not a QEMU source tree: %s\n' "$QEMU_SRC" >&2
    exit 1
fi

# Keep the link out of `git status` in the QEMU checkout.
EXCLUDE=
if command -v git > /dev/null 2>&1; then
    EXCLUDE="$(git -C "$QEMU_SRC" rev-parse --path-format=absolute \
        --git-path info/exclude 2> /dev/null || true)"
fi

add_line() {
    grep -qxF -- "$2" "$1" && return 0
    # Do not join the line onto a last line without a newline.
    if [ -s "$1" ] && [ -n "$(tail -c 1 -- "$1")" ]; then
        printf '\n' >> "$1"
    fi
    printf '%s\n' "$2" >> "$1"
}

remove_line() {
    grep -qxF -- "$2" "$1" || return 0
    local kept
    kept="$(grep -vxF -- "$2" "$1" || true)"
    printf '%s\n' "$kept" > "$1"
}

if [ "$REMOVE" = 1 ]; then
    if [ -L "$LINK" ]; then
        rm -- "$LINK"
    elif [ -e "$LINK" ]; then
        printf '%s is not a link; leaving it in place.\n' "$LINK" >&2
        exit 1
    fi
    remove_line "$MESON" "$MESON_LINE"
    remove_line "$KCONFIG" "$KCONFIG_LINE"
    if [ -n "$EXCLUDE" ] && [ -f "$EXCLUDE" ]; then
        remove_line "$EXCLUDE" /hw/ldd
    fi
    printf 'Removed the ldd devices from %s\n' "$QEMU_SRC"
    exit 0
fi

if [ -L "$LINK" ] && [ "$(readlink -- "$LINK")" = "$DEVICES_DIR" ]; then
    :
elif [ -e "$LINK" ] || [ -L "$LINK" ]; then
    printf '%s already exists and does not link to %s\n' "$LINK" "$DEVICES_DIR" >&2
    exit 1
else
    ln -s -- "$DEVICES_DIR" "$LINK"
fi
add_line "$MESON" "$MESON_LINE"
add_line "$KCONFIG" "$KCONFIG_LINE"
if [ -n "$EXCLUDE" ]; then
    mkdir -p -- "$(dirname -- "$EXCLUDE")"
    touch -- "$EXCLUDE"
    add_line "$EXCLUDE" /hw/ldd
fi
printf 'Linked %s into %s\n' "$DEVICES_DIR" "$QEMU_SRC"
