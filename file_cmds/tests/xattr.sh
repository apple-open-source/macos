#!/bin/sh

# Regression test for 79795987
function regression_79795987()
{
	echo "Verifying that xattr does not exit cleanly on failures"

	dir_name=`mktemp -d /tmp/XXXXXX`

	set +e
	/usr/bin/xattr -p com.apple.xattrtest.idontexist ${dir_name} 2> /dev/null
	exit_status=$?
	set -e

	if [[ $exit_status -ne 1 ]]; then
		echo "xattr exited with unexpected status $exit_status, expected 1"
		exit 1
	fi
}

# Regression test for 79990691
function regression_79990691()
{
	echo "Verifying that xattr behaves correctly when hex values have embedded NULs"

	dir_name=`mktemp -d /tmp/XXXXXX`

	/usr/bin/xattr -w -x com.apple.xattrtest "00 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00" ${dir_name}
}

# Regression test for 80300336
function regression_80300336()
{
	echo "Verifying that deleting an xattr recursively does not exit with an error"

	dir_name=`mktemp -d /tmp/XXXXXX`
	subdir_name=`mktemp -d ${dir_name}/XXXXXX`
	file_name=`mktemp ${subdir_name}/XXXXXX`

	/usr/bin/xattr -d -r com.apple.xattrtest ${dir_name}
}

# Regression test for 82573904
function regression_82573904()
{
	echo "Verifying that deleting an xattr recursively with a symlink directory target does not affect files in that directory"

	dir_name=`mktemp -d /tmp/XXXXXX`
	subdir_name=`mktemp -d ${dir_name}/XXXXXX`
	file_name=`mktemp ${subdir_name}/XXXXXX`

	# Write the xattr first
	/usr/bin/xattr -w com.apple.xattrtest testvalue ${file_name}

	# Ensure we can read it back
	/usr/bin/xattr -p com.apple.xattrtest ${file_name} > /dev/null

	# Now create a symlink to ${subdir_name} inside ${dir_name}
	symlink_name="xattrtestsymlink"
	subdir_basename=`basename ${subdir_name}`
	cd ${dir_name} && /bin/ln -s ${subdir_basename} ${symlink_name}

	# Delete recursively on the symlink path
	/usr/bin/xattr -d -r com.apple.xattrtest ${dir_name}/${symlink_name}

	# xattr should still be there
	/usr/bin/xattr -p com.apple.xattrtest ${file_name} > /dev/null
}

set -eu -o pipefail

regression_79795987
regression_79990691
regression_80300336
regression_82573904

exit 0
