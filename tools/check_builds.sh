#!/usr/bin/env bash
# Build every example (kernel modules and userspace tests) against one kernel tree
# through the top-level examples/Makefile, in parallel.
# Usage: tools/check_builds.sh [KDIR] [EXAMPLE...]
set -u

if [ -n "${1:-}" ]; then
    KDIR="$1"
    shift
else
    : "${LDD_ROOT:?Pass a kernel tree path or export LDD_ROOT first.}"
    KDIR="$LDD_ROOT/src/linux-stable"
fi
if [ ! -f "$KDIR/Makefile" ]; then
    printf 'Not a kernel tree: %s\n' "$KDIR" >&2
    exit 2
fi
KDIR="$(cd -- "$KDIR" && pwd -P)"

EXAMPLES="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../examples" && pwd -P)"
# Build in a scratch copy so the checkout stays clean.
WORK="$(mktemp -d "${TMPDIR:-/tmp}/ldd-build.XXXXXX")"
trap 'rm -rf -- "$WORK"' EXIT
cp -R -- "$EXAMPLES/." "$WORK/"

targets=()
failed=()
for name in "$@"; do
    if [ -d "$WORK/$name" ]; then
        targets+=("$name")
    else
        printf 'No such example: %s\n' "$name" >&2
        failed+=("$name")
    fi
done
# With no names, the top-level Makefile builds every finished example.
if [ "$#" -eq 0 ] || [ "${#targets[@]}" -gt 0 ]; then
    # -k keeps going past failures; -O keeps each directory's log together.
    make -C "$WORK" -k -O -j"$(nproc)" --no-print-directory \
        KDIR="$KDIR" ${targets[@]+"${targets[@]}"} 2>&1 | grep -E -v '^make(\[[0-9]+\])?: '
fi | tee "$WORK/.log"

passed=$(grep -c '^PASS ' "$WORK/.log")
while read -r _ rel; do failed+=("$rel"); done < <(grep '^FAIL ' "$WORK/.log")

printf '\n%d built, %d failed against %s\n' "$passed" "${#failed[@]}" "$KDIR"
for name in ${failed[@]+"${failed[@]}"}; do printf '  %s\n' "$name"; done
[ "${#failed[@]}" -eq 0 ]
