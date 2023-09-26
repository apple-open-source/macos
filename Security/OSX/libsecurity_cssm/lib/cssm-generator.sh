#!/bin/sh

TARGET="${BUILT_PRODUCTS_DIR}/derived_src"
CONFIG="${PROJECT_DIR}/OSX/libsecurity_cssm/lib/generator.cfg"

mkdir -p "${TARGET}"
/usr/bin/perl "${PROJECT_DIR}/OSX/libsecurity_cssm/lib/generator.pl" "${SRCROOT}/OSX/libsecurity_cssm/lib/" "${CONFIG}" "${TARGET}"
