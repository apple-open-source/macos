#!/bin/sh

print_tcpdump_version()
{
	cat tcpdump/VERSION
}


print_darwin_version()
{
	darwin_version="${RC_ProjectSourceVersion}"
	if [ -z ${darwin_version} ]; then
		darwin_version="`git branch|grep '* '|awk '{ print $2 }'`"
	fi
	echo ${darwin_version}
}

version_string="tcpdump version `print_tcpdump_version` -- Apple version `print_darwin_version`"

mkdir -p "${SHARED_DERIVED_FILE_DIR}"

echo "static const char tcpdump_version_string[] = \"${version_string}\";" > "${SHARED_DERIVED_FILE_DIR}/tcpdump_version.h"

exit 0
