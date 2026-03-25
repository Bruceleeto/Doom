#!/bin/bash
# Asset pipeline: extract Doom 3 pk4s into base_mod/
# Usage: ./tools/assets.sh [extract|clean]

set -e

SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
BASEDIR="base"
MODDIR="base_mod"

cmd_extract() {
    echo "=== Extracting pk4 files from ${BASEDIR}/ into ${MODDIR}/ ==="
    mkdir -p "${MODDIR}"

    for pk4 in "${BASEDIR}"/*.pk4; do
        [ -f "$pk4" ] || continue
        echo "  Extracting $(basename "$pk4")..."
        unzip -oq "$pk4" -d "${MODDIR}"
    done

    # fix permissions from pk4 extraction
    chmod -R u+w "${MODDIR}"

    echo "=== Done ==="
}

cmd_clean() {
    echo "=== Cleaning ${MODDIR}/ ==="
    rm -rf "${MODDIR}"
    echo "=== Done ==="
}

case "${1:-extract}" in
    extract) cmd_extract ;;
    clean)   cmd_clean ;;
    *)       echo "Usage: $0 [extract|clean]"; exit 1 ;;
esac
