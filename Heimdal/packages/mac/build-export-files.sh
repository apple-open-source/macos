#/bin/sh

mkdir -p "${HEIMDAL_TMPDIR}"

function exportfile() {
	output="$1" ; shift
	input="$@"

	( echo '#include "config.h"'

	for a in ${input} ; do
		while read x ; do
			case "$x" in
				'#'*) continue;;
				%*) echo $x | sed 's/%/#/' ;;
				'&'*) echo $(echo "_$x" | sed 's/&//g') ;;
				*,private) echo $(echo "$x" | sed 's/\(.*\),private/___ApplePrivate_\1/') ;;
				*) echo "_$x" ;;
			esac
		done <  ${SOURCE_ROOT}/packages/mac/framework/${a}.sym
	done ) | cc -E -I"${SRCROOT}/packages/mac/SnowLeopard10A" ${CFLAGS} ${HEIMDAL_PLATFORM_CFLAGS} -DNO_CONFIG_INCLUDE=1 - >  $output.new

	if cmp "${output}.new" "${output}" 2> /dev/null; then
		rm "${output}.new"
	else
		cp "${output}.new" "${output}"
	fi
}

function rewritefile() {
	output="$1" ; shift
	input="$@"

	echo "rewrite file"

	for a in ${input} ; do
		while read x ; do
			case "$x" in
				*,private) a=$(echo "$x" | sed 's/\(.*\),private/\1/') ; echo "#define ${a} __ApplePrivate_${a}" ;;
			esac
		done <  ${SOURCE_ROOT}/packages/mac/framework/${a}.sym
	done >  $output.new

	if cmp "${output}.new" "${output}" 2> /dev/null; then
		rm "${output}.new"
	else
		cp "${output}.new" "${output}"
	fi
}


exportfile "${HEIMDAL_TMPDIR}/heimdal.exp" hcrypto roken krb5 hx509 pkinit asn1 base
exportfile "${HEIMDAL_TMPDIR}/gss.exp" gss gss-oid
exportfile "${HEIMDAL_TMPDIR}/heimdal-asn1.exp" heimdal-asn1
exportfile "${HEIMDAL_TMPDIR}/commonauth.exp" commonauth
exportfile "${HEIMDAL_TMPDIR}/heimodadmin.exp" heimodadmin

rewritefile "${HEIMDAL_TMPDIR}/gssapi_rewrite.h" gss

exit 0
