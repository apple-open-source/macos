#!/bin/sh
set -e

MERGE_FILE=authorization.merge
ETC_DIR=${DSTROOT}/private/etc
SRC=${SRCROOT}/etc

if [ -f ${SRC}/${MERGE_FILE} ]; then
    echo "Installing ${MERGE_FILE}..."
    mkdir -p ${ETC_DIR}
    plutil -lint ${SRC}/${MERGE_FILE}
    cp ${SRC}/${MERGE_FILE} ${ETC_DIR}/${MERGE_FILE}
else 
    echo "file not found: ${SRC}/${MERGE_FILE}"
fi

