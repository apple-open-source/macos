#!/bin/sh

FILES="hcrypto roken krb5 hx509 pkinit gss asn1 base"

mkdir -p "${HEIMDAL_TMPDIR}"

for base in ${FILES} ; do

	input="${SOURCE_ROOT}/packages/mac/framework/${base}.sym"
	output="${HEIMDAL_TMPDIR}/sym_${base}.c"

	sh ${SOURCE_ROOT}/packages/mac/make-export.sh ${input} "${output}.new" || exit 1
	if cmp "${output}.new" "${output}" 2> /dev/null; then
		rm "${output}.new"
	else
		echo "xcode: Updating ${output}"
		mv "${output}.new" "${output}"
	fi

done

exit 0
