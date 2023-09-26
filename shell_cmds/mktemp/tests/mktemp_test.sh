#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Klara Systems
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

atf_test_case tmpdir_env
tmpdir_env_body()
{

	tmpdir="$PWD"

	atf_check -o match:"^$tmpdir/foo\..+$" \
	    env TMPDIR="$tmpdir" mktemp -t foo
}

atf_test_case tmpdir_pflag
tmpdir_pflag_body()
{

	mkdir tmp_p tmp_env

	tmpdir="$PWD/tmp_env"
	export TMPDIR="$tmpdir"

	pflag="$PWD/tmp_p"

	# Basic usage: just -p specified
	atf_check -o match:"^$pflag/tmp\..+$" \
	    env -u TMPDIR mktemp -p "$pflag"
	atf_check -o match:"^$pflag/tmp\..+$" \
	    env TMPDIR="$tmpdir" mktemp -p "$pflag"

	# -p with a list of names
	atf_check -o ignore env -u TMPDIR mktemp -p "$pflag" x y z
	atf_check test -f "$pflag/x"
	atf_check test -f "$pflag/y"
	atf_check test -f "$pflag/z"

	# Checking --tmpdir usage, which should defer to $TMPDIR followed by
	# /tmp with no value specified.
#ifndef __APPLE__
#	atf_check -o match:"^/tmp/foo\..+$" \
#	    env -u TMPDIR mktemp --tmpdir -t foo
#endif
	atf_check -o match:"^$tmpdir/foo\..+$" \
	    env TMPDIR="$tmpdir" mktemp --tmpdir -t foo

	# Finally, combined -p -t
	atf_check -o match:"^$pflag/foo\..+$" \
	    env -u TMPDIR mktemp -p "$pflag" -t foo
	atf_check -o match:"^$pflag/foo\..+$" \
	    env TMPDIR="$tmpdir" mktemp -p "$pflag" -t foo
}

atf_test_case tmpdir_pflag_dir
tmpdir_pflag_dir_body()
{

	tmpdir="$PWD"
	atf_check -o save:tmpname \
	    env -u TMPDIR mktemp -d -p "$tmpdir" -t foo

	# Better diagnostics when using -o match: + cat rather than grep.
	atf_check -o match:"^$tmpdir/foo\..+$" cat tmpname
	cdir=$(cat tmpname)

	atf_check test -d "$cdir"

	atf_check -o match:"^$tmpdir/footmp$" \
	    env -u TMPDIR mktemp -d -p "$tmpdir" footmp
	atf_check test -d "$tmpdir/footmp"
}

atf_test_case tmpdir_pflag_noarg
tmpdir_pflag_noarg_body()
{

	# Without -t, this time; this introduces $TMPDIR without having to use
	# it.
	tmpdir="$PWD"
	atf_check -o save:tmpname \
	    env TMPDIR="$tmpdir" mktemp --tmpdir foo.XXXXXXXX
	atf_check -o match:"^$tmpdir/foo\..+$" cat tmpname

	# An empty string gets the same treatment.
	atf_check -o save:tmpname \
	    env TMPDIR="$tmpdir" mktemp -p '' foo.XXXXXXXX
	atf_check -o match:"^$tmpdir/foo\..+$" cat tmpname
}

atf_test_case tmpdir_tflag_oneslash
tmpdir_tflag_oneslash_body()
{

#ifdef __APPLE__
	# This is a little bit of a hack; using this as $TMPDIR won't actually
	# have any effect, but it's the neater patch to make.
	usertemp="$(getconf DARWIN_USER_TEMP_DIR)"
	tmpdir=${usertemp%%/}
#else
#	tmpdir="$PWD"
#endif

	# Provided a trailing slash, we shouldn't end up with two trailing
	# slashes.
	atf_check -o save:tmpname \
	    env TMPDIR="$tmpdir/" mktemp -t foo
	atf_check -o match:"^$tmpdir/foo\..+$" cat tmpname
}

#ifdef __APPLE__
atf_test_case darwin_usertemp
darwin_usertemp_body()
{
	# mktemp(1) on macOS is patched to use _CS_DARWIN_USER_TEMP_DIR if it's
	# set, preferred over TMPDIR.  We use -u for all mktemp(1) invocations
	# here to avoid needing a cleanup routine; it's simpler this way, as we
	# can't specify a cleanup program to exec in the test manifest.
	usertemp="$(getconf DARWIN_USER_TEMP_DIR)"
	tmpdir="$(realpath .)/"

	atf_check -o match:"^${usertemp}foo.+$" \
	    env TMPDIR="$tmpdir" mktemp -ut foo
	atf_check -o match:"^${usertemp}tmp.+$" \
	    env TMPDIR="$tmpdir" mktemp -u

	# With -p, precedence is flipped so that we honor the following order:
	# - -p argument, if set and not empty
	atf_check -o match:"^${tmpdir}tmp.+$" \
	    env -u TMPDIR mktemp -up "${tmpdir}"
	# - TMPDIR, if set
	atf_check -o match:"^${tmpdir}tmp.+$" \
	    env TMPDIR="$tmpdir" mktemp -up ''
	# - _CS_DARWIN_USER_TEMP_DIR, if set
	atf_check -o match:"^${usertemp}tmp.+$" \
	    env -u TMPDIR mktemp -up ''
	# - _PATH_TMP as last resort, but we can't really unset
	#     _CS_DARWIN_USER_TEMP_DIR to test that.
}
#endif

atf_init_test_cases()
{
#ifndef __APPLE__
	# Apple's mktemp(1) uses _CS_DARWIN_USER_TEMP_DIR, which is non-trivial
	# to bypass for testing of $TMPDIR.
#	atf_add_test_case tmpdir_env
#endif
	atf_add_test_case tmpdir_pflag
	atf_add_test_case tmpdir_pflag_dir
	atf_add_test_case tmpdir_pflag_noarg
	atf_add_test_case tmpdir_tflag_oneslash
#ifdef __APPLE__
	atf_add_test_case darwin_usertemp
#endif
}
