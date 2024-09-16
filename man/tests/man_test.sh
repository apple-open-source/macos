#
# Copyright (c) 2023 Apple Inc. All rights reserved.
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

fake_manpage()
{
	local path="$1"
	local name="${2:-CONTENT}"
	local desc="${3:-Description}"

	cat <<EOF > "$path"
.Dd January 1, 1970
.Dt $name 1
.Os
.Sh NAME
.Nm $name ,
.Nm "MAN $name"
.Nd $desc
EOF
}

# Construct a mandir with content(1) and a "man content"(1)
construct_mandir()
{
	mandir=$1

	mkdir -p "$mandir"/man1
	fake_manpage "$mandir"/man1/content.1

	ln -s content.1 "$mandir"/man1/"man content.1"
}

atf_test_case spaces
spaces_body()
{

	construct_mandir "man"
	construct_mandir "project man"

	# Sanity check
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man ./man/man1/content.1

	# Spaces in direct path to man(1)
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man ./man/man1/"man content.1"
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man "./project man/man1/content.1"
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man "./project man/man1/man content.1"

	# Sanity check our MANPATH
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man -M "$PWD/man" content

	# Space in page name in MANPATH
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man -M "$PWD/man" "man content"

	# Space in MANPATH name
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man -M "$PWD/project man" "content"
	atf_check -o match:"CONTENT" \
	    env MANPAGER="cat" man -M "$PWD/project man" "man content"
}

atf_test_case custom_mansect
custom_mansect_body()
{

	# man_default_sections doesn't have, e.g., 1p in it.  Test that that
	# works now, and we'll also make sure that.
	mkdir -p man/man1 man/man1p man/man1q

	fake_manpage man/man1p/cmd.1p
	cp man/man1p/cmd.1p man/man1q/other.1q
	cp man/man1p/cmd.1p man/man1/finally.1z

	# Using a non built-in MANSECT, but it's in man.conf.  These two don't
	# seem to work with the previous GNU man, it looks like it's expecting
	# 1<foo> to be in man1 rathern than its own directory; we'll make sure
	# that works, but also allow them to be in their own man1<foo>.
	atf_check -o match:"CONTENT" \
	    env MANPATH="$PWD/man" MANPAGER="cat" man cmd

	# Using an unlisted man section in the arguments
	atf_check -o match:"CONTENT" \
	    env MANPATH="$PWD/man" MANPAGER="cat" man 1q other

	# We should find it in a the base section number, too
	atf_check -o match:"CONTENT" \
	    env MANPATH="$PWD/man" MANPAGER="cat" man 1z finally

	# Regression tests for rdar://124959653
	# Don't check for extended section after regular section
	atf_check -s exit:1 -e match:"for 1p$" \
	    env MANPATH="$PWD/man" MANPAGER="cat" man 1 1p cmd
	# Narrow down extended section syntax to /^[0-9][a-z]{1,3}$/
	atf_check -o match:"CONTENT" -e match:"for 1p1$" \
	    env MANPATH="$PWD/man" MANPAGER="cat" man 1p1 cmd
	atf_check -o match:"CONTENT" -e match:"for 1pabc$" \
	    env MANPATH="$PWD/man" MANPAGER="cat" man 1pabc cmd
}

atf_test_case whatis
whatis_body()
{
	mkdir -p manA/man1 manB/man8
	fake_manpage manA/man1/cmd.1 cmd "Command"
	fake_manpage manA/man1/other.1 other "Other command"
	echo "cmd(1), MAN cmd(1)     - Command" >manA/whatis
	echo "other(1), MAN other(1) - Other command" >>manA/whatis
	fake_manpage manB/man8/cmd.8 cmd "Extra command"
	fake_manpage manB/man8/stuff.8 stuff "Stuff"
	atf_check -o save:out1 \
	    env MANPATH="$PWD/manA:$PWD/manB" MANPAGER="cat" \
	    man -S1 -k cmd
	atf_check -o match:1 wc -l <out1
	atf_check -o match:1 grep -c cmd <out1
	atf_check -o save:out18 \
	    env MANPATH="$PWD/manA:$PWD/manB" MANPAGER="cat" \
	    man -S1:8 -k cmd
	atf_check -o match:2 wc -l <out18
	atf_check -o match:2 grep -c cmd <out18
	atf_check -o save:out8 \
	    env MANPATH="$PWD/manA:$PWD/manB" MANPAGER="cat" \
	    man -S8 -k cmd
	atf_check -o match:1 wc -l <out8
	atf_check -o match:1 grep -c cmd <out8
}

atf_test_case so
so_body()
{
	mkdir -p man/man1 man/man8
	fake_manpage man/man1/cmd.1 cmd "Command"
	atf_check -o match:"Command" env MANPATH="$PWD/man" man cmd
	# correct usage
	echo ".so man1/cmd.1" >man/man8/foo.8
	atf_check -o match:"Command" env MANPATH="$PWD/man" man foo
	# incorrect but widespread usage
	echo ".so cmd.1" >man/man1/bar.1
	atf_check -o match:"Command" env MANPATH="$PWD/man" man bar
}

atf_init_test_cases()
{

	atf_add_test_case spaces
	atf_add_test_case custom_mansect
	atf_add_test_case whatis
	atf_add_test_case so
}
