#!/bin/sh

set -ex

DSTROOT=${DSTROOT}${INSTALL_PATH_PREFIX}

mkdir -m 0755 -p ${DSTROOT}/usr/local/OpenSourceLicenses ${DSTROOT}/usr/local/OpenSourceVersions
install -m 0444 ${SRCROOT}/bzip2.plist ${DSTROOT}/usr/local/OpenSourceVersions/bzip2.plist
install -m 0444 ${SRCROOT}/bzip2/LICENSE ${DSTROOT}/usr/local/OpenSourceLicenses/bzip2.txt

