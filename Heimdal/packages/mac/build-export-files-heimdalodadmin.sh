#/bin/sh

mkdir -p "${HEIMDAL_TMPDIR}"

exportfile() {
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

exportfile "${HEIMDAL_TMPDIR}/heimodadmin.exp" heimodadmin

exit 0
