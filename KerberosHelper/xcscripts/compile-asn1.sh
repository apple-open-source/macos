#!/bin/sh

set -e

dir=$(mktemp -d ${TEMP_FILE_DIR}/asn1-compileXXXXXXX)

cd $dir || exit 1

bname=$(echo ${INPUT_FILE_BASE} | sed -e 's/-/_/g' )

xcrun heimdal-asn1_compile \
    --type-file=inttypes.h \
    --template --no-parse-units --one-code-file \
    ${SCRIPT_INPUT_FILE} ${bname}_asn1

odir=$(dirname ${SCRIPT_OUTPUT_FILE_1})
test -d ${odir} || mkdir ${odir}

cp asn1_${bname}_asn1.x ${SCRIPT_OUTPUT_FILE_0}
cp ${bname}_asn1.hx ${SCRIPT_OUTPUT_FILE_1}
cp ${bname}_asn1-priv.hx ${SCRIPT_OUTPUT_FILE_2}

rm -r ${dir}
