#!/bin/sh
#
# Variant links cannot be created in the actual target, because Strip/CodeSign/etc are
# after all other phases. Running it in the aggregate target guarantees that the variants
# are really linked to the actual stripped/signed binary.
#

set -ex

ln ${DSTROOT}/usr/bin/pkill ${DSTROOT}/usr/bin/pgrep
ln ${DSTROOT}/usr/share/man/man1/pkill.1 ${DSTROOT}/usr/share/man/man1/pgrep.1
