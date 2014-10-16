#!/bin/sh

set -ex

DSTROOT=${DSTROOT}${INSTALL_PATH_PREFIX}

# This script phase is run here, so that the hardlinks are created *after* stripping.
# Doing it in the bzip2 target itself produces verification failures.
ln ${DSTROOT}/usr/bin/bzip2 ${DSTROOT}/usr/bin/bunzip2
ln ${DSTROOT}/usr/bin/bzip2 ${DSTROOT}/usr/bin/bzcat

