#!/bin/sh

set -ex

DSTROOT="$DSTROOT$INSTALL_PATH_PREFIX"

mv ${DSTROOT}/usr/local/OpenSourceLicenses/COPYING ${DSTROOT}/usr/local/OpenSourceLicenses/libarchive.txt

