#!/bin/sh
#
# Make a built Max external self-contained.
#
# A freshly linked kernel.mxo refers to its ZMQ and OpenSSL dependencies by
# absolute Homebrew path (/opt/homebrew/... on Apple silicon, /usr/local/... on
# Intel). Such an external only loads on a machine that has Homebrew installed
# at exactly that prefix, which makes it undistributable.
#
# This script copies every non-system dependency -- transitively -- into
# Contents/Frameworks and rewrites the install names to @rpath, so the external
# resolves them from inside its own bundle.
#
# Usage: bundle_dylibs.sh <path-to-bundle.mxo> [codesign-identity]

set -eu

BUNDLE="${1:?usage: bundle_dylibs.sh <bundle.mxo> [identity]}"
IDENTITY="${2:--}"

BUNDLE_NAME=$(basename "$BUNDLE")
BINARY_NAME="${BUNDLE_NAME%.mxo}"
BINARY="$BUNDLE/Contents/MacOS/$BINARY_NAME"
FRAMEWORKS="$BUNDLE/Contents/Frameworks"

if [ ! -f "$BINARY" ]; then
    echo "bundle_dylibs: no binary at $BINARY" >&2
    exit 1
fi

# List the install names a Mach-O file depends on, excluding its own id line
# and anything shipped with macOS.
external_deps() {
    otool -L "$1" | tail -n +2 | awk '{print $1}' | while read -r dep; do
        case "$dep" in
            /usr/lib/*|/System/*|@rpath/*|@loader_path/*|@executable_path/*)
                continue
                ;;
        esac
        # Skip a library's own id (it appears first when the file is a dylib).
        if [ "$(basename "$dep")" = "$(basename "$1")" ]; then
            continue
        fi
        echo "$dep"
    done
}

mkdir -p "$FRAMEWORKS"

# Breadth-first copy of the whole dependency closure.
WORKLIST=$(external_deps "$BINARY")
COPIED=""

while [ -n "$WORKLIST" ]; do
    NEXT=""
    for dep in $WORKLIST; do
        name=$(basename "$dep")

        case " $COPIED " in
            *" $name "*) continue ;;
        esac

        if [ ! -f "$dep" ]; then
            echo "bundle_dylibs: missing dependency $dep" >&2
            exit 1
        fi

        cp -f "$dep" "$FRAMEWORKS/$name"
        chmod u+w "$FRAMEWORKS/$name"
        install_name_tool -id "@rpath/$name" "$FRAMEWORKS/$name" 2>/dev/null

        COPIED="$COPIED $name"
        NEXT="$NEXT $(external_deps "$FRAMEWORKS/$name")"
    done
    WORKLIST="$NEXT"
done

if [ -z "$COPIED" ]; then
    echo "bundle_dylibs: nothing to bundle (already self-contained)"
    exit 0
fi

# Repoint every reference -- in the external and in the copied libraries
# themselves -- at the bundled copies.
retarget() {
    target="$1"
    otool -L "$target" | tail -n +2 | awk '{print $1}' | while read -r dep; do
        case "$dep" in
            /usr/lib/*|/System/*|@rpath/*|@loader_path/*|@executable_path/*)
                continue
                ;;
        esac
        name=$(basename "$dep")
        case " $COPIED " in
            *" $name "*)
                install_name_tool -change "$dep" "@rpath/$name" "$target" 2>/dev/null
                ;;
        esac
    done
}

add_rpath_once() {
    target="$1"
    rpath="$2"
    if otool -l "$target" | grep -A2 LC_RPATH | grep -q " path $rpath "; then
        return 0
    fi
    install_name_tool -add_rpath "$rpath" "$target" 2>/dev/null || true
}

retarget "$BINARY"
add_rpath_once "$BINARY" "@loader_path/../Frameworks"

for name in $COPIED; do
    retarget "$FRAMEWORKS/$name"
    # Sibling libraries live next to each other inside Frameworks.
    add_rpath_once "$FRAMEWORKS/$name" "@loader_path"
done

# Rewriting load commands invalidates any existing signature, so sign last.
for name in $COPIED; do
    codesign -s "$IDENTITY" -f "$FRAMEWORKS/$name" 2>/dev/null || true
done
codesign -s "$IDENTITY" -f --deep "$BUNDLE" 2>/dev/null || true

echo "bundle_dylibs: bundled$COPIED into $BUNDLE_NAME"
