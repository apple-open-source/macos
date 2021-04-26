#!/bin/sh

# Regression test for 69452380
function regression_69452380()
{
	echo "Verifying that cp -p preserves symlink's attributes, rather than the attributes of the symlink's target."
	test_dir=`mktemp -d /tmp/69452380_src_XXXX`
	touch ${test_dir}/target
	mkdir ${test_dir}/link_dir
	# Create symlink (must use relative path for the failure to occur)
	cd ${test_dir}/link_dir
	ln -s ../target link
	# cp (with attribute preservation) the test dir containing both the
	# target and the link (in a subdirectory) to a new dir.
	# Prior to 69452380, this failed because we followed the symlink to
	# try and preserve attributes for a non-existing file, instead of
	# preserving the attributes of the symlink itself.
	cp -R -P -p ${test_dir} /tmp/69452380_tgt_${RANDOM}
}

set -eu -o pipefail

regression_69452380
