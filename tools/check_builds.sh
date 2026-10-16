#!/usr/bin/env bash
# Build every example (kernel modules and userspace tests) against one kernel tree.
# Usage: tools/check_builds.sh [KERNELDIR] [EXAMPLE...]
set -u

if [ -n "${1:-}" ]; then
    KERNELDIR="$1"
    shift
else
    : "${LDD_ROOT:?Pass a kernel tree path or export LDD_ROOT first.}"
    KERNELDIR="$LDD_ROOT/src/linux-stable"
fi
if [ ! -f "$KERNELDIR/Makefile" ]; then
    printf 'Not a kernel tree: %s\n' "$KERNELDIR" >&2
    exit 2
fi
KERNELDIR="$(cd -- "$KERNELDIR" && pwd -P)"

EXAMPLES="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../examples" && pwd -P)"
# Build in a scratch copy so the checkout stays clean.
WORK="$(mktemp -d "${TMPDIR:-/tmp}/ldd-build.XXXXXX")"
trap 'rm -rf -- "$WORK"' EXIT
cp -R -- "$EXAMPLES/." "$WORK/"

if [ "$#" -gt 0 ]; then
    names=("$@")
else
    names=()
    # Unfinished examples are skipped unless named explicitly.
    for dir in "$WORK"/*/; do
        case "$dir" in *_NOT_COMPLETE/) continue ;; esac
        names+=("$(basename -- "$dir")")
    done
fi

failed=()
count=0
for name in "${names[@]}"; do
    if [ ! -d "$WORK/$name" ]; then
        printf 'No such example: %s\n' "$name" >&2
        count=$((count + 1))
        failed+=("$name")
        continue
    fi
    # Sorted, so a module builds before subdirectories that use its Module.symvers.
    while IFS= read -r makefile; do
        dir="$(dirname -- "$makefile")"
        rel="${dir#"$WORK"/}"
        count=$((count + 1))
        if log="$(make -C "$dir" KERNELDIR="$KERNELDIR" 2>&1)"; then
            printf 'PASS %s\n' "$rel"
        else
            printf 'FAIL %s\n%s\n' "$rel" "$log"
            failed+=("$rel")
        fi
    done < <(find "$WORK/$name" -name Makefile | LC_ALL=C sort)
done

printf '\n%d built, %d failed against %s\n' "$((count - ${#failed[@]}))" "${#failed[@]}" "$KERNELDIR"
for name in ${failed[@]+"${failed[@]}"}; do printf '  %s\n' "$name"; done
[ "${#failed[@]}" -eq 0 ]
