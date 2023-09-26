#
# SPDX-License-Identifier: APSL-1.0
#
# Copyright (c) 2022 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
#
# @APPLE_LICENSE_HEADER_END@
#

atf_test_case backup_policy
backup_policy_body()
{
	printf "a\nb\nc\nd\ne\nf\ng\nh\ni\n" > foo
	printf "a\nb\nc\nx\nx\nx\ng\nh\ni\n" > foo_new
	printf "x\nb\nc\nd\nx\nf\ng\nh\nx\n" > foo_ng

	printf "z\nb\nc\nd\ne\nf\ng\nh\nz\n" > boo
	printf "a\nb\nc\nx\nx\nx\ng\nh\ni\n" > zoo

	diff -u foo foo_new > foo.diff
	diff -u1 foo foo_ng > foong.diff

	# Apply the patch to foo; this matches exactly, so we should not create
	# a .orig file.
	atf_check -o ignore patch foo foo.diff
	atf_check test '!' -f foo.orig

	printf "a\nb\nc\nd\ne\nf\ng\nh\ni\n" > foo

	# Applying foo.diff to boo requires fuzz, so it should create a backup.
	atf_check -o ignore patch boo foo.diff
	atf_check test -f boo.orig
	mv boo.orig boo

	# zoo requires no fuzz, but the middle hunk will fail.  We should then
	# create a backup.
	atf_check -o ignore -s not-exit:0 patch zoo foong.diff
	atf_check test -f zoo.orig
	mv zoo.orig zoo
}

atf_test_case backup_nomismatch
backup_nomismatch_body()
{
	printf "a\nb\nc\nd\ne\nf\ng\nh\ni\n" > foo
	printf "a\nb\nc\nx\nx\nx\ng\nh\ni\n" > foo_new
	printf "x\nb\nc\nd\nx\nf\ng\nh\nx\n" > foo_ng

	printf "z\nb\nc\nd\ne\nf\ng\nh\nz\n" > boo
	printf "a\nb\nc\nx\nx\nx\ng\nh\ni\n" > zoo

	diff -u foo foo_new > foo.diff
	diff -u1 foo foo_ng > foong.diff

	# Applying foo.diff to boo requires fuzz, so it would usually create a
	# backup.
	atf_check -o ignore patch --no-backup-if-mismatch boo foo.diff
	atf_check test '!' -f boo.orig

	# zoo requires no fuzz, but the middle hunk will fail.  We should still
	# not create a backup.
	atf_check -o ignore -s not-exit:0 patch --no-backup-if-mismatch \
	    zoo foong.diff
	atf_check test '!' -f zoo.orig
}

atf_test_case backup_override
backup_override_body()
{
	printf "a\nb\nc\nd\ne\nf\ng\nh\ni\n" > foo
	printf "a\nb\nc\nx\nx\nx\ng\nh\ni\n" > foo_new

	diff -u foo foo_new > foo.diff

	# Apply the patch to foo; this matches exactly, but -b means we should
	# always create the backup.
	atf_check -o ignore patch -b foo foo.diff
	atf_check test -f foo.orig
}

atf_test_case backup_posix
backup_posix_body()
{
	printf "a\nb\nc\nd\ne\nf\ng\nh\ni\n" > foo
	printf "a\nb\nc\nx\nx\nx\ng\nh\ni\n" > foo_new

	printf "z\nb\nc\nd\ne\nf\ng\nh\nz\n" > boo

	diff -u foo foo_new > foo.diff
	cp boo boo.created

	# Required fuzz, but with --posix we shouldn't have created a backup.
	atf_check -o ignore patch --posix boo foo.diff
	atf_check test '!' -f boo.orig
	cp boo.created boo

	atf_check -o ignore env POSIXLY_CORRECT=yes patch boo foo.diff
	atf_check test '!' -f boo.orig
	cp boo.created boo

	# --backup-if-mismatch should still work if specified manually...
	atf_check -o ignore patch --posix --backup-if-mismatch boo foo.diff
	atf_check test -f boo.orig
	mv boo.orig boo

	# ... in both orders.
	atf_check -o ignore patch --backup-if-mismatch --posix boo foo.diff
	atf_check test -f boo.orig
	mv boo.orig boo

	# It should also still work in the face of POSIXLY_CORRECT.
	atf_check -o ignore env POSIXLY_CORRECT=yes \
	    patch --backup-if-mismatch boo foo.diff
	atf_check test -f boo.orig
}

atf_test_case basename_prefix
basename_prefix_body()
{
	mkdir -p foo/zoo
	echo "A\nB\nC\nD\nE\nF" > bar
	echo "A\nB\nx\nx\nE\nF" > bar.out
	cp bar foo/bar
	cp bar foo/zoo/bar

	diff -u bar bar.out > bar.diff

	# First: simple, no slashes
	atf_check -o ignore patch -V simple -Y 0_ bar bar.diff
	atf_check test -f 0_bar.orig

	# Second: one slash
	atf_check -o ignore patch -V simple -Y 1_ foo/bar bar.diff
	atf_check test -f foo/1_bar.orig

	# Finally: multiple slashes
	atf_check -o ignore patch -V simple -Y 2_ foo/zoo/bar bar.diff
	atf_check test -f foo/zoo/2_bar.orig
}

atf_test_case tz_support_basic_context
tz_support_basic_context_body()
{
	printf "A\nB\nC\nD\nE\nF\nG\nH\nI\n" > my_file
	printf "O\nB\nC\nD\nE\nF\nG\nH\nO\n" > my_file_fuzzy

	# Many of these tests will want to coerce to a local timezone, so set
	# to MST to avoid daylight savings time.
	export TZ="MST"

	# Arbitrary time in the past, clock is unlikely to be this far off.
	# The time*utc variables are for -Z matching.
	time1="2022-01-01T12:00:00Z"
	time2="2022-01-01T16:00:00Z"
	timefmt="%Y-%m-%dT%H:%M:%SZ"

	touch -md "$time1" my_file
	touch -md "$time2" my_file_patched

	cp -c my_file my_file.orig

	atf_check -o save:my_file.diff -s ignore diff -c my_file my_file_patched

	# Forward setting, interpret as local
	atf_check -o ignore patch -T my_file my_file.diff
	atf_check -o match:"$time2" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file

	# Reversing should do the expected, the 'new' file mtime is what we
	# compare against and if successful, we set the output file to the 'old'
	# file mtime.
	atf_check -o ignore patch -RT my_file my_file.diff
	atf_check -o match:"$time1" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file

	# More exhaustive testing happens in the tz_support_basic_unified test.
}

atf_test_case tz_support_basic_unified
tz_support_basic_unified_body()
{
	printf "A\nB\nC\nD\nE\nF\nG\nH\nI\n" > my_file
	printf "O\nB\nC\nD\nE\nF\nG\nH\nO\n" > my_file_fuzzy
	printf "A\nB\nC\nX\nX\nX\nG\nH\nI\n" > my_file_patched

	# Many of these tests will want to coerce to a local timezone, so set
	# to MST to avoid daylight savings time.
	export TZ="MST"
	export COMMAND_MODE="unix2003"

	# Arbitrary time in the past, clock is unlikely to be this far off.
	# The time*utc variables are for -Z matching.
	time1="2022-01-01T12:00:00Z"
	time1utc="2022-01-01T05:00:00Z"
	time2="2022-01-01T16:00:00Z"
	time2utc="2022-01-01T09:00:00Z"
	timefmt="%Y-%m-%dT%H:%M:%SZ"

	touch -md "$time1" my_file
	touch -md "$time1" my_file_fuzzy
	touch -md "$time2" my_file_patched

	cp -c my_file my_file.orig

	atf_check -o save:my_file.diff -s ignore diff -u my_file my_file_patched

	if head -1 my_file.diff | grep -qv '05:00:00$'; then
		atf_expect_fail \
		    "This test assumes POSIX conformant unified timestamps"
	fi

	# Forward setting, interpret as local
	atf_check -o ignore patch -T my_file my_file.diff
	atf_check -o match:"$time2" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file

	# Reversing should do the expected, the 'new' file mtime is what we
	# compare against and if successful, we set the output file to the 'old'
	# file mtime.
	atf_check -o ignore patch -RT my_file my_file.diff
	atf_check -o match:"$time1" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file

	# Time mismatch, the output's time shouldn't be set.
	touch -mA "010203" my_file
	atf_check -o ignore patch -T my_file my_file.diff
	atf_check -o not-match:"$time2" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file

	# Reset, now let's try -Z.  We'll only try forward with this one, as the
	# two time options share the forward/reverse logic.
	cp -p my_file.orig my_file
	touch -md "$time1utc" my_file

	atf_check -o ignore patch -Z my_file my_file.diff
	atf_check -o match:"$time2utc" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file
}

atf_test_case tz_support_force
tz_support_force_body()
{
	printf "A\nB\nC\nD\nE\nF\nG\nH\nI\n" > my_file
	printf "O\nB\nC\nD\nE\nF\nG\nH\nO\n" > my_file_fuzzy
	printf "A\nB\nC\nX\nX\nX\nG\nH\nI\n" > my_file_patched

	# Many of these tests will want to coerce to a local timezone, so set
	# to MST to avoid daylight savings time.
	export TZ="MST"
	export COMMAND_MODE="unix2003"

	# Arbitrary time in the past, clock is unlikely to be this far off.
	time1="2022-01-01T12:00:00Z"
	time2="2022-01-01T16:00:00Z"
	timefmt="%Y-%m-%dT%H:%M:%SZ"

	touch -md "$time1" my_file
	touch -md "$time1" my_file_fuzzy
	touch -md "$time2" my_file_patched

	atf_check -o save:my_file.diff -s ignore diff -u my_file my_file_patched

	# my_file_fuzzy will require fuzz, thus shouldn't adjust the time
	# without force.
	mv my_file_fuzzy my_file
	cp -p my_file my_file.orig

	atf_check -o ignore \
	    patch --no-backup-if-mismatch -T my_file my_file.diff
	atf_check -o not-match:"$time2" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file

	# Now revert and force it.  Sanity check the backup file's mtime.
	atf_check -o match:"$time1" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file.orig
	cp -p my_file.orig my_file

	atf_check -o ignore \
	    patch --no-backup-if-mismatch -fT my_file my_file.diff
	atf_check -o match:"$time2" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file
}

atf_test_case tz_support_creation
tz_support_creation_body()
{
	printf "A\nB\nC\nD\nE\nF\nG\nH\nI\n" > my_file

	# Many of these tests will want to coerce to a local timezone, so set
	# to MST to avoid daylight savings time.
	export TZ="MST"

	# Arbitrary time in the past, clock is unlikely to be this far off.
	time1="2022-01-01T12:00:00Z"
	timefmt="%Y-%m-%dT%H:%M:%SZ"

	touch -md "$time1" my_file

	# Write a diff that creates the file, then delete it and check the
	# mtime.
	atf_check -o save:my_file.diff -s ignore diff -u /dev/null my_file
	rm my_file

	atf_check -o ignore \
	    patch --no-backup-if-mismatch -T my_file my_file.diff
	atf_check -o match:"$time1" \
	    env TZ=UTC stat -f '%Sm' -t "$timefmt" my_file
}

atf_test_case vcs_support
vcs_support_body()
{
	# Note that we can't really test this, because we don't have any of the
	# clients.  We can do a dry-run of RCS and make sure that -g 0 works.
	printf "A\nB\nC\nD\nE\nF\n" > my_file
	printf "A\X\nX\nX\nX\nF\n" > my_file_patched

	cp my_file my_file.save

	diff -u my_file my_file_patched > my_file.diff
	atf_check -o ignore -x 'patch -g0 < my_file.diff'

	mkdir -p rcs/RCS
	cd rcs

	touch RCS/my_file,v
	atf_check -o match:"my_file does not exist, but was found in RCS." \
	    -s not-exit:0 -x 'env PATCH_GET=1 patch -C < ../my_file.diff'

	cp ../my_file.save my_file
	atf_check -o match:"patching file my_file" \
	    -x 'patch -C -g1 < ../my_file.diff'
}

atf_test_case eof_patch
eof_patch_body()
{

	atf_check cp "$(atf_get_srcdir)"/eof_patch.txt .

	atf_check -o match:"patching file eof_patch.txt" \
	    patch -N eof_patch.txt "$(atf_get_srcdir)"/eof_patch.patch
	atf_check -o match:"Ignoring previously applied" -s exit:1 \
	    patch -N eof_patch.txt "$(atf_get_srcdir)"/eof_patch.patch

	# Now generate a context diff and test that.
	atf_check -s exit:1 -x \
	    "diff -C3 \"$(atf_get_srcdir)/eof_patch.txt\" eof_patch.txt > \
	    eof_patch.patch"

	atf_check cp "$(atf_get_srcdir)"/eof_patch.txt .
	atf_check -o match:"patching file eof_patch.txt" \
	    patch -N eof_patch.txt "$(atf_get_srcdir)"/eof_patch.patch
	atf_check -o match:"Ignoring previously applied" -s exit:1 \
	    patch -N eof_patch.txt "$(atf_get_srcdir)"/eof_patch.patch

}


atf_test_case bof_patch
bof_patch_body()
{

	atf_check cp "$(atf_get_srcdir)"/bof_patch.txt .

	atf_check -o match:"patching file bof_patch.txt" \
	    patch -N bof_patch.txt "$(atf_get_srcdir)"/bof_patch.patch
	atf_check -o match:"Ignoring previously applied" -s exit:1 \
	    patch -N bof_patch.txt "$(atf_get_srcdir)"/bof_patch.patch

	# Now generate a context diff and test that.
	atf_check -s exit:1 -x \
	    "diff -C3 \"$(atf_get_srcdir)/bof_patch.txt\" bof_patch.txt > \
	    bof_patch.patch"

	atf_check cp "$(atf_get_srcdir)"/bof_patch.txt .
	atf_check -o match:"patching file bof_patch.txt" \
	    patch -N bof_patch.txt "$(atf_get_srcdir)"/bof_patch.patch
	atf_check -o match:"Ignoring previously applied" -s exit:1 \
	    patch -N bof_patch.txt "$(atf_get_srcdir)"/bof_patch.patch
}

atf_test_case quiet
quiet_body()
{
	printf "A\nB\nC\nD\nE\nF\n" > my_file
	printf "A\X\nX\nX\nX\nF\n" > my_file_patched

	cp my_file my_file.save

	diff -u my_file my_file_patched > my_file.diff

	atf_check -o match:"patching file my_file" \
	    patch my_file my_file.diff

	cp my_file.save my_file
	atf_check patch -s my_file my_file.diff
}

atf_test_case bof_100350977
bof_100350977_body()
{
	srcdir="$(atf_get_srcdir)"
	inf="$srcdir"/bof_100350977.in
	outf="$srcdir"/bof_100350977.out

	atf_check -s exit:1 -o save:ctx.diff diff -c "$inf" "$outf"
	atf_check patch -so file.out "$inf" ctx.diff

	atf_check cmp -s "$outf" file.out
}

atf_test_case utf16
utf16_body()
{
	# rdar://problem/101531623 - UTF-16LE breaks patch(1)
	printf "A\nHello\nB\n" > file1.ascii
	printf "A\nHello, world\nB\n" > file2.ascii

	# Further testing revealed that UTF-16 broke patch(1) differently.  With
	# UTF-16LE, patch(1) succeeds but the result is wrong.  With UTF-16, the
	# patch(1) fails entirely.
	for difftype in ctx dflt uni; do
		dflag=""
		case ${difftype} in
			ctx)
				dflag="-c"
				;;
			dflt)
				dflag=""
				;;
			uni)
				dflag="-u"
				;;
			*)
				atf_fail "Unhandled type ${difftype}"
				;;
		esac

		for encoding in "" "LE"; do
			FLAVOR="${difftype}${Encoding}"
			atf_check -o save:file1${encoding} \
			    iconv -f ASCII -t UTF-16${encoding} file1.ascii
			atf_check -o save:file2${encoding} \
			    iconv -f ASCII -t UTF-16${encoding} file2.ascii

			atf_check -o save:file${FLAVOR}.diff -s exit:1 \
			    diff -a ${dflag} file1${encoding} file2${encoding}

			# Sanity check, diff should have a comma in it...
			atf_check -o not-empty grep ',' file${FLAVOR}.diff

			atf_check -o not-empty \
			    patch file1${encoding} file${FLAVOR}.diff
			atf_check -o file:file2${encoding} cat file1${encoding}
		done
	done
}

atf_test_case noeol
noeol_body()
{

	atf_check -o not-empty patch -o out \
	    $(atf_get_srcdir)/noeol.in $(atf_get_srcdir)/noeol.patch
	atf_check cmp -s out $(atf_get_srcdir)/noeol.out
}

atf_test_case rejfile
rejfile_body()
{
	atf_check -o save:expected_uni.rej \
	    tail -n +3 "$(atf_get_srcdir)/rejfile_foo_uni.diff"
	atf_check -o save:expected_ctx.rej \
	    tail -n +3 "$(atf_get_srcdir)/rejfile_foo_ctx.diff"

	atf_check cp "$(atf_get_srcdir)/rejfile_foo.in" foo.in

	atf_check -o not-empty -s exit:1 \
	    patch foo.in "$(atf_get_srcdir)/rejfile_foo_uni.diff"
	atf_check -o file:expected_uni.rej cat foo.in.rej

	atf_check -o not-empty -s exit:1 \
	    patch foo.in "$(atf_get_srcdir)/rejfile_foo_ctx.diff"
	atf_check -o file:expected_ctx.rej cat foo.in.rej
}

atf_test_case ed_sh_comments
ed_sh_comments_body()
{

	# rdar://problem/103217456 - patch(1) doesn't apply lines with sh-style
	# comments

	printf "a\nb\nc\n" > foo.orig
	printf "# 1\n# 2\nc\n" > foo

	atf_check -o save:foo.diff -s exit:1 diff --ed foo.orig foo

	mv foo foo.expected
	mv foo.orig foo

	atf_check -o empty -s exit:0 patch --ed -o foo.recover foo foo.diff
	atf_check cmp -s foo.recover foo.expected
}

atf_init_test_cases()
{
	atf_add_test_case backup_policy
	atf_add_test_case backup_nomismatch
	atf_add_test_case backup_override
	atf_add_test_case backup_posix
	atf_add_test_case basename_prefix
	atf_add_test_case tz_support_basic_context
	atf_add_test_case tz_support_basic_unified
	atf_add_test_case tz_support_creation
	atf_add_test_case tz_support_force
	atf_add_test_case vcs_support
	atf_add_test_case eof_patch
	atf_add_test_case bof_patch
	atf_add_test_case quiet
	atf_add_test_case bof_100350977
	atf_add_test_case utf16
	atf_add_test_case noeol
	atf_add_test_case rejfile
	atf_add_test_case ed_sh_comments
}
