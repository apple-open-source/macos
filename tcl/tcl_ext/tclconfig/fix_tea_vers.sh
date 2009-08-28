#!/bin/bash
eval $(awk '/^[^d].*TEA_VERSION=/' $(dirname $0)/tcl.m4)
if [ -z "${TEA_VERSION}" ]; then
    echo "Cannot determine tcl.m4 TEA_VERSION" >&2 && exit 1; fi
for f in "$@"; do
    sed -e "s/^TEA_INIT(\[.*\])/TEA_INIT([${TEA_VERSION}])/" $f > $f.1 &&
    mv -f $f.1 $f
done
