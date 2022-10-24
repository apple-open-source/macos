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

setup_files()
{
	printf "ABCDEFGHI" > foo
	printf "ABCXXXGHI" > bar
	diff -u foo bar > foo.diff
	cp foo foo.orig

	cp foo 'foo space'
	cp foo foo$'\t'tab
	cp foo "foo'squote"
	cp foo "foo\"dquote"
	cp foo "foo 'both quotes\""
	cp foo foo_underscore
}

atf_test_case literal
literal_body()
{
	setup_files

	export PATCH_VERBOSE=1

	atf_check -o match:"Patching file foo using Plan" \
	    patch --quoting-style=literal foo foo.diff
	atf_check -o match:$'Patching file foo\ttab using Plan' \
	    patch --quoting-style=literal $'foo\ttab' foo.diff
	atf_check -o match:"Patching file foo'squote using Plan" \
	    patch --quoting-style=literal "foo'squote" foo.diff
	atf_check -o match:"Patching file foo\"dquote using Plan" \
	    patch --quoting-style=literal "foo\"dquote" foo.diff
	# ATF isn't quote safe, apparently...
#	atf_check -o match:"Patching file foo\'both quotes\" using Plan" \
#	    patch --quoting-style=literal "foo\'both quotes\"" foo.diff
	atf_check -o match:"Patching file foo_underscore using Plan" \
	    patch --quoting-style=literal "foo_underscore" foo.diff
}

atf_test_case shell
shell_body()
{
	setup_files

	export PATCH_VERBOSE=1

	# shell
	atf_check -o match:"Patching file foo using Plan" \
	    patch --quoting-style=shell foo foo.diff
	atf_check -o match:"Patching file 'foo'[$]'[\\]t''tab' using Plan" \
	    patch --quoting-style=shell $'foo\ttab' foo.diff
	atf_check -o match:"Patching file \"foo'squote\" using Plan" \
	    patch --quoting-style=shell "foo'squote" foo.diff
	atf_check -o match:"Patching file 'foo\"dquote' using Plan" \
	    patch --quoting-style=shell 'foo"dquote' foo.diff
	# ATF isn't quote safe, apparently...
#	atf_check -o match:"Patching file \"foo\\'both quotes\\\"\" using Plan" \
#	    patch --quoting-style=shell "foo\'both quotes\"" foo.diff
	atf_check -o match:"Patching file foo_underscore using Plan" \
	    patch --quoting-style=shell "foo_underscore" foo.diff

	# shell as the default
	atf_check -o match:"Patching file foo using Plan" \
	    patch foo foo.diff
	atf_check -o match:"Patching file 'foo'[$]'[\\]t''tab' using Plan" \
	    patch $'foo\ttab' foo.diff
	atf_check -o match:"Patching file \"foo'squote\" using Plan" \
	    patch "foo'squote" foo.diff
	atf_check -o match:"Patching file 'foo\"dquote' using Plan" \
	    patch 'foo"dquote' foo.diff
	# ATF isn't quote safe, apparently...
#	atf_check -o match:"Patching file \"foo\\'both quotes\\\"\" using Plan" \
#	    patch "foo\'both quotes\"" foo.diff
	atf_check -o match:"Patching file foo_underscore using Plan" \
	    patch "foo_underscore" foo.diff

	# shell-always
	atf_check -o match:"Patching file 'foo' using Plan" \
	    patch --quoting-style=shell-always foo foo.diff
	atf_check -o match:"Patching file 'foo'[$]'[\\]t''tab' using Plan" \
	    patch --quoting-style=shell-always $'foo\ttab' foo.diff
	atf_check -o match:"Patching file \"foo'squote\" using Plan" \
	    patch --quoting-style=shell-always "foo'squote" foo.diff
	atf_check -o match:"Patching file 'foo\"dquote' using Plan" \
	    patch --quoting-style=shell-always 'foo"dquote' foo.diff
	# ATF isn't quote safe, apparently...
#	atf_check -o match:"Patching file \"foo\\'both quotes\\\"\" using Plan" \
#	    patch --quoting-style=shell-always "foo\'both quotes\"" foo.diff
	atf_check -o match:"Patching file 'foo_underscore' using Plan" \
	    patch --quoting-style=shell-always "foo_underscore" foo.diff

	# shell-always (via QUOTING_STYLE)
	export QUOTING_STYLE="shell-always"
	atf_check -o match:"Patching file 'foo' using Plan" \
	    patch foo foo.diff
	atf_check -o match:"Patching file 'foo'[$]'[\\]t''tab' using Plan" \
	    patch $'foo\ttab' foo.diff
	atf_check -o match:"Patching file \"foo'squote\" using Plan" \
	    patch "foo'squote" foo.diff
	atf_check -o match:"Patching file 'foo\"dquote' using Plan" \
	    patch 'foo"dquote' foo.diff
	# ATF isn't quote safe, apparently...
#	atf_check -o match:"Patching file \"foo\\'both quotes\\\"\" using Plan" \
#	    patch "foo\'both quotes\"" foo.diff
	atf_check -o match:"Patching file 'foo_underscore' using Plan" \
	    patch "foo_underscore" foo.diff
}

atf_test_case cstyle
cstyle_body()
{
	setup_files

	export PATCH_VERBOSE=1

	# C
	atf_check -o match:"Patching file \"foo\" using Plan" \
	    patch --quoting-style=c foo foo.diff
	atf_check -o match:"Patching file \"foo[\\]ttab\" using Plan" \
	    patch --quoting-style=c $'foo\ttab' foo.diff
	atf_check -o match:"Patching file \"foo'squote\" using Plan" \
	    patch --quoting-style=c "foo'squote" foo.diff
	atf_check -o match:"Patching file \"foo[\\]\"dquote\" using Plan" \
	    patch --quoting-style=c 'foo"dquote' foo.diff
	# ATF isn't quote safe, apparently...
#	atf_check -o match:"Patching file \"foo\\'both quotes\\\"\" using Plan" \
#	    patch --quoting-style=c "foo\'both quotes\"" foo.diff
	atf_check -o match:"Patching file \"foo_underscore\" using Plan" \
	    patch --quoting-style=c "foo_underscore" foo.diff

	# Escape (C w/o quotes)
	atf_check -o match:"Patching file foo using Plan" \
	    patch --quoting-style=escape foo foo.diff
	atf_check -o match:"Patching file foo[\\]ttab using Plan" \
	    patch --quoting-style=escape $'foo\ttab' foo.diff
	atf_check -o match:"Patching file foo'squote using Plan" \
	    patch --quoting-style=escape "foo'squote" foo.diff
	atf_check -o match:"Patching file foo[\\]\"dquote using Plan" \
	    patch --quoting-style=escape 'foo"dquote' foo.diff
	# ATF isn't quote safe, apparently...
#	atf_check -o match:"Patching file \"foo\\'both quotes\\\"\" using Plan" \
#	    patch --quoting-style=escape "foo\'both quotes\"" foo.diff
	atf_check -o match:"Patching file foo_underscore using Plan" \
	    patch --quoting-style=escape "foo_underscore" foo.diff
}

atf_init_test_cases()
{
	atf_add_test_case literal
	atf_add_test_case shell
	atf_add_test_case cstyle
}
