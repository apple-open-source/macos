#!/bin/sh

#RC_ProjectSourceVersion=1.2.3
#RC_ProjectSourceVersion=9999.99.99
#RC_ProjectNameAndSourceVersion=tcpdump-Branch.eng_PR_123456789__623e4dd2d9945f007629c0c7801b418635791e13
#RC_ProjectNameAndSourceVersion=tcpdump-Branch.SHA__ea89f6fda992afd6cd6fec108722c18034564220

print_darwin_version()
{
	echo ${darwin_version}
}

if [ -z "${RC_ProjectSourceVersion}" ]; then
	if [ -f .git/config ]; then
		grep -q tcpdump.git .git/config
		if [ $? -eq 0 ]; then
			darwin_version="`git status|head -n 1|awk '{ print $NF }'` (`date '+%Y-%m-%d %H:%M:%S'`)"
		fi
	fi
elif [ "${RC_ProjectSourceVersion}" = "9999.99.99" ]; then
	echo ${RC_ProjectNameAndSourceVersion}|grep -q "^tcpdump-Branch.*__"
	if [ $? -eq 0 ]; then
		branch="`echo ${RC_ProjectNameAndSourceVersion}|sed s/^tcpdump-Branch.*__//`"
		if [ -n "${branch}" ]; then
			darwin_version="${branch}"
		fi
	fi
else
	darwin_version="${RC_ProjectSourceVersion}"
fi

if [ -z "${darwin_version}" ]; then
	darwin_version="main (`date '+%Y-%m-%d %H:%M:%S'`)"
fi

version_string="Apple version `print_darwin_version`"

if [ -z "${SHARED_DERIVED_FILE_DIR}" ]; then
	echo ${version_string}
	exit 0
fi

mkdir -p "${SHARED_DERIVED_FILE_DIR}"

echo "static const char apple_version_string[] = \"${version_string}\";" > "${SHARED_DERIVED_FILE_DIR}/tcpdump_version.h"

exit 0
