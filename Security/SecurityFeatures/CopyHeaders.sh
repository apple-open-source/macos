#!/bin/sh

set -x

BASEDIR=$(dirname $0)

source=${1}

TARGET_DIR="${BUILT_PRODUCTS_DIR}/include/Security"

test -d "${TARGET_DIR}" || mkdir -p "${TARGET_DIR}"

cp "${BASEDIR}/${source}/SecurityFeatures.h" "${TARGET_DIR}"
