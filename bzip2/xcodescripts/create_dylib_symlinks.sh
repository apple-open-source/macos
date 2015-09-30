#!/bin/sh

set -ex

ln -fs libbz2.1.0.dylib ${DSTROOT}/usr/lib/libbz2.dylib
ln -fs libbz2.1.0.dylib ${DSTROOT}/usr/lib/libbz2.1.0.5.dylib

