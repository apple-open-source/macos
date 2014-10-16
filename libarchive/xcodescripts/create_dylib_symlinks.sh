#!/bin/sh

set -ex

DSTROOT="$DSTROOT$INSTALL_PATH_PREFIX"

ln -s libarchive.2.dylib ${DSTROOT}/usr/lib/libarchive.dylib

