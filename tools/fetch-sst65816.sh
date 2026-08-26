#!/bin/sh
#
# Fetch the SingleStepTests/65816 data set for src/tests/sst65816.cpp.
#
# The full set is 512 files (one per opcode per CPU mode) totalling ~3 GB, so
# it is not vendored. With no arguments this downloads all of it; otherwise it
# downloads only the named opcodes.
#
#   tools/fetch-sst65816.sh                 # everything, into sst65816/v1
#   tools/fetch-sst65816.sh 3d 48 cb        # just these opcodes
#
# Set SST65816_DIR to download somewhere other than ./sst65816.

set -eu

BASE=https://raw.githubusercontent.com/SingleStepTests/65816/main/v1
DIR="${SST65816_DIR:-sst65816}/v1"

if ! command -v curl >/dev/null 2>&1; then
    echo "curl is required" >&2
    exit 1
fi

mkdir -p "$DIR"

if [ $# -eq 0 ]; then
    opcodes=$(awk 'BEGIN { for (i = 0; i < 256; i++) printf "%02x\n", i }')
else
    opcodes=$(printf '%s\n' "$@" | tr 'A-Z' 'a-z')
fi

for op in $opcodes; do
    for mode in e n; do
        name="$op.$mode.json"
        if [ -s "$DIR/$name" ]; then
            continue
        fi
        echo "fetching $name"
        curl -fsSL --retry 3 -o "$DIR/$name.part" "$BASE/$name"
        mv "$DIR/$name.part" "$DIR/$name"
    done
done

echo "test data in $DIR"
