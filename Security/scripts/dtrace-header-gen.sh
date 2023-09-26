#!/bin/bash -ex
mkdir -p "$(dirname "$2")"
xcrun usdtheadergen -C -s "$1" -o "$2"
