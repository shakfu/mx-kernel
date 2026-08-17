#!/bin/sh
#
# Re-apply the local patches carried against the vendored dependencies.
#
# The vendored trees under source/projects/kernel/thirdparty are checked in as
# plain files, so a patch applied to them looks like ordinary source. Refreshing
# a dependency silently reverts it. Run this after any such refresh.
#
# Each patch is idempotent: if it is already applied, the patch is skipped
# rather than reported as a failure.

set -eu

PATCH_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$PATCH_DIR/.." && pwd)
THIRDPARTY="$ROOT/source/projects/kernel/thirdparty"

apply_one() {
    patch_file="$1"
    target_dir="$2"

    if [ ! -d "$target_dir" ]; then
        echo "skip $(basename "$patch_file"): $target_dir not present"
        return 0
    fi

    if patch -d "$target_dir" -p1 --dry-run --force --silent < "$patch_file" >/dev/null 2>&1; then
        patch -d "$target_dir" -p1 --force --silent < "$patch_file"
        echo "applied $(basename "$patch_file")"
        return 0
    fi

    if patch -d "$target_dir" -p1 -R --dry-run --force --silent < "$patch_file" >/dev/null 2>&1; then
        echo "already applied $(basename "$patch_file")"
        return 0
    fi

    echo "FAILED $(basename "$patch_file"): does not apply to $target_dir" >&2
    echo "The vendored source has diverged. Re-derive the patch by hand." >&2
    return 1
}

status=0

apply_one "$PATCH_DIR/xeus-zmq-0001-iopub-welcome-parent-header.patch" \
          "$THIRDPARTY/xeus-zmq" || status=1

apply_one "$PATCH_DIR/xeus-zmq-0002-cmake-policy-range.patch" \
          "$THIRDPARTY/xeus-zmq" || status=1

apply_one "$PATCH_DIR/xeus-zmq-0003-timed-poll-and-idle-callback.patch" \
          "$THIRDPARTY/xeus-zmq" || status=1

apply_one "$PATCH_DIR/xeus-0002-cmake-policy-range.patch" \
          "$THIRDPARTY/xeus" || status=1

exit $status
