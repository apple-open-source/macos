#!/bin/sh
#
# usage: pbxbuild.sh [options] [assignments...]
#     options:  -target <tname>         Build only the target named <tname>
#               -buildstyle <bsname>    Use the build style named <bsname>
#

USAGE_TEXT='usage: pbxbuild.sh [options] [actions...] [assignments...]
     options:  -target <tname>         Build only the target named <tname>
               -buildstyle <bsname>    Use the build style named <bsname>'


Target=
BuildStyle=
Actions=
Assignments=

# We change into the directory that contains the pbxbuild.sh script.
cd "`dirname -- "$0"`"

IFS='
'

while [ $# -gt 0 ] ; do
	case ${1} in
		-h | -help)
			echo "$USAGE_TEXT"
			exit 0
			;;
#		-target)
#			shift
#			if [ ! -z ${1} ]; then
#				Target="${1}"
#			fi
#			shift
#			;;
#		-buildstyle)
#			shift
#			if [ ! -z ${1} ]; then
#				BuildStyle="${1}"
#			fi
#			shift
#			;;
		-*)
			echo "pbxbuild: invalid option '"${1}"'"
			echo "$USAGE_TEXT"
			exit 1
			;;
		*=*)
			if [ -z ${Assignments} ]; then
				Assignments="${1}"
			else
				Assignments="${Assignments}${IFS}${1}"
			fi
			shift
			;;
		*)
			if [ -z ${Actions} ]; then
				Actions="${1}"
			else
				Actions="${Actions}${IFS}${1}"
			fi
			shift
			;;
	esac
done

# Set up access paths
DataPath=pbxbuild.data

# The default action is 'build'
if [ -z "${Actions}" ]; then
	Actions=build
fi

# Set up target information
. "${DataPath}/TargetNames"

for Action in ${Actions}; do
	for Target in ${Targets}; do
		TargetPath="${DataPath}/${Target}.build"
		echo
		echo "*** ${Action} ${Target} ***"
		echo jam -d2 ${Action} JAMFILE=\"${TargetPath}/Jamfile.jam\" JAMBASE=pbxbuild.data/ProjectBuilderJambase TARGETNAME=\"${Target}\" ACTION=${Action} OS=darwin NATIVE_ARCH=`arch` SRCROOT=\"`pwd`\" OBJROOT=\"`pwd`/obj\" SYMROOT=\"`pwd`/sym\" DSTROOT=\"`pwd`/dst\" ${Assignments}
		jam -d2 ${Action} JAMFILE="${TargetPath}/Jamfile.jam" JAMBASE=pbxbuild.data/ProjectBuilderJambase TARGETNAME="${Target}" ACTION=${Action} OS=darwin NATIVE_ARCH=`arch` SRCROOT="`pwd`" OBJROOT="`pwd`/obj" SYMROOT="`pwd`/sym" DSTROOT="`pwd`/dst" ${Assignments}
		[ $? != 0 ] && exit 1
	done
done

exit 0
