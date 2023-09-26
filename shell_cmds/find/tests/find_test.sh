#
# Copyright 2017, Conrad Meyer <cem@FreeBSD.org>.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

#ifdef __APPLE__
# These tests are not hooked up because they check for behavior that is not
# conformant.  Notably, operands that accept paths (e.g., -newer, -samefile)
# should not exhibit different behavior under -H/-L/-P.
#endif

atf_test_case find_newer_link
find_newer_link_head()
{
	atf_set "descr" "Verifies that -newer correctly uses a symlink, " \
	    "rather than its target, for comparison"
}
find_newer_link_body()
{
	atf_check -s exit:0 mkdir test
	atf_check -s exit:0 ln -s file1 test/link
	atf_check -s exit:0 touch -d 2017-12-31T10:00:00Z -h test/link
	atf_check -s exit:0 touch -d 2017-12-31T11:00:00Z test/file2
	atf_check -s exit:0 touch -d 2017-12-31T12:00:00Z test/file1

	# find(1) should evaluate 'link' as a symlink rather than its target
	# (with -P / without -L flags).  Since link was created first, the
	# other two files should be newer.
	echo -e "test\ntest/file1\ntest/file2" > expout
	atf_check -s exit:0 -o save:output find test -newer test/link
	atf_check -s exit:0 -o file:expout sort < output
}

atf_test_case find_samefile_link
find_samefile_link_head()
{
	atf_set "descr" "Verifies that -samefile correctly uses a symlink, " \
	    "rather than its target, for comparison"
}
find_samefile_link_body()
{
	atf_check -s exit:0 mkdir test
	atf_check -s exit:0 touch test/file3
	atf_check -s exit:0 ln -s file3 test/link2

	# find(1) should evaluate 'link' as a symlink rather than its target
	# (with -P / without -L flags).
	atf_check -s exit:0 -o "inline:test/link2\n" find test -samefile test/link2
}

#ifdef __APPLE__
atf_test_case newerBm_msprec
newerBm_msprec_body()
{

	# -newerBm should be able to pick up the difference in birth time
	# between two files created within milliseconds of each other.  Earlier
	# versions of find(1) truncated birthtime to just the tv_sec component,
	# while newer versions compare tv_nsec if the two files were created
	# within the same second.  Thus, it's imperative to reproduce this that
	# we both create the files within milliseconds of each other, but also
	# that those milliseconds not cross a second boundary.
	mkdir -p scratch
	while true; do
		touch baseline
		touch scratch/file_a

		base=$(stat -f '%m' baseline)
		new_a=$(stat -f '%m' scratch/file_a)

		if [ "$base" -eq "$new_a" ]; then
			break
		else
			rm baseline scratch/file_a
		fi
	done

	# This one's guaranteed to show up with this delay, so we're not worried
	# about it.
	sleep 1
	touch scratch/file_b

	cat <<EOF > expected
scratch/file_a
scratch/file_b
EOF

	atf_check -o file:expected find scratch -type f -newerBm baseline
}
#endif

atf_init_test_cases()
{
	atf_add_test_case find_newer_link
	atf_add_test_case find_samefile_link
#ifdef __APPLE__
	atf_add_test_case newerBm_msprec
#endif
}
