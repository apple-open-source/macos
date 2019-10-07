#!/bin/sh

set -ex

if [[ "${ACTION}" == "install" ]]; then
  ln -s libexpat.1.dylib ${DSTROOT}/usr/lib/libexpat.dylib
fi

if [[ "${ACTION}" == "install" || "${ACTION}" == "installapi" ]]; then
  ln -s libexpat.1.tbd ${DSTROOT}/usr/lib/libexpat.tbd
fi
