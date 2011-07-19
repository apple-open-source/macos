#!/bin/sh
set -e
set -x

OSV="$DSTROOT"/usr/local/OpenSourceVersions
OSL="$DSTROOT"/usr/local/OpenSourceLicenses

mkdir -p "$OSV" "$OSL"

cp "$SRCROOT"/local/vim.plist "$OSV"/vim.plist
cp "$SRCROOT"/runtime/doc/uganda.txt "$OSL"/vim.txt
