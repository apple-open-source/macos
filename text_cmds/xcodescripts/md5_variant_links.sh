#!/bin/sh
#
# This script phase cannot be run in the "md5" target itself; see
# grep_variant_links.sh for the rationale.
#

set -ex

for variant in md5sum sha1 sha1sum sha224 sha224sum sha256 sha256sum sha384 \
    sha384sum sha512 sha512sum; do
    ln -f ${DSTROOT}/sbin/md5 ${DSTROOT}/sbin/${variant}
    ln -f ${DSTROOT}/usr/share/man/man1/md5.1 ${DSTROOT}/usr/share/man/man1/${variant}.1
done
