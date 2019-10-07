#!/bin/bash -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0
[ "$ACTION" == "installapi" ] && exit 0

mkdir -m 0755 -p ${DSTROOT}/AppleInternal/Tests/libarchive
install -m 0755 ${SRCROOT}/tests/*.sh ${DSTROOT}/AppleInternal/Tests/libarchive
install -m 0644 ${SRCROOT}/tests/*.tar ${DSTROOT}/AppleInternal/Tests/libarchive
install -m 0644 ${SRCROOT}/libarchive/tar/test/*.uu ${DSTROOT}/AppleInternal/Tests/libarchive
install -m 0644 ${SRCROOT}/libarchive/libarchive/test/*.uu ${DSTROOT}/AppleInternal/Tests/libarchive
install -m 0644 ${SRCROOT}/libarchive/cpio/test/*.uu ${DSTROOT}/AppleInternal/Tests/libarchive
