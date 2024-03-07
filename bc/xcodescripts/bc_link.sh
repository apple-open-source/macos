#!/bin/sh
#
# This script phase cannot be run in the "bc" target itself, because Strip/CodeSign/etc are
# after all other phases. Running it in the aggregate target guarantees that dc
# is really linked to the actual stripped/signed bc binary.
#

set -ex

ln -f ${DSTROOT}/usr/bin/bc ${DSTROOT}/usr/bin/dc
