#!/bin/bash -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0
[ "$ACTION" == "installapi" ] && exit 0

# This script phase is run here, so that the hardlinks are created *after* stripping.
# Doing it in the bzip2 target itself produces verification failures.
ln ${DSTROOT}/usr/bin/bzip2 ${DSTROOT}/usr/bin/bunzip2
ln ${DSTROOT}/usr/bin/bzip2 ${DSTROOT}/usr/bin/bzcat

