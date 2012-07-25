#!/bin/sh

set -ex

ln -s libexpat.1.dylib ${DSTROOT}/usr/lib/libexpat.dylib
ln -s libexpat.1.dylib ${DSTROOT}/usr/lib/libexpat.1.5.2.dylib
