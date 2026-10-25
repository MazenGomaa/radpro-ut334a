#!/bin/sh
# Copy the public UT334A docs and tools into the Rad Pro fork (radpro/ut334a/).
# The workspace copies are the master; never copy backup/ or analysis/ (private).
set -e
cd "$(dirname "$0")/.."
dst=radpro/ut334a
mkdir -p "$dst/docs" "$dst/tools"
cp -r docs/. "$dst/docs/"
cp -r tools/. "$dst/tools/"
cp fm33-stlink.cfg "$dst/"
