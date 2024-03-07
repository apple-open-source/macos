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

atf_test_case builtin
builtin_body()
{

	# The builtin rules maps everything to UTF-8, so just try a couple of
	# different samples.
	atf_check -o match:"UTF-8" \
	    env LC_ALL=C $(atf_get_srcdir)/print_charset
	atf_check -o match:"UTF-8" \
	    env LC_ALL=en_US.US-ASCII $(atf_get_srcdir)/print_charset
	atf_check -o match:"UTF-8" \
	    env LC_ALL=en_US.UTF-8 $(atf_get_srcdir)/print_charset
	atf_check -o match:"UTF-8" \
	    env LC_ALL=en_US.ISO8859-1 $(atf_get_srcdir)/print_charset

	# Also specifically ensure that an empty codeset still gets mapped to
	# UTF-8.
	atf_check -o match:"UTF-8" \
	    env LIBCHARSET_CODESET= $(atf_get_srcdir)/print_charset
}

atf_test_case csalias
csalias_body()
{

	# Mappings are case sensitive
	cat <<EOF > charset.alias
ASCII UPPERCASE
ascii lowercase
* OTHERS
EOF

	export CHARSETALIASDIR=$PWD
	atf_check -o match:"UPPERCASE" \
	    env LIBCHARSET_CODESET=ASCII $(atf_get_srcdir)/print_charset
	atf_check -o match:"lowercase" \
	    env LIBCHARSET_CODESET=ascii $(atf_get_srcdir)/print_charset
	atf_check -o match:"OTHERS" \
	    env LIBCHARSET_CODESET=AsCiI $(atf_get_srcdir)/print_charset
}

atf_test_case empty
empty_body()
{

	export CHARSETALIASDIR=$PWD

	# Missing file is treated as an empty map, which overrides our default
	# of UTF-8
	atf_check -o match:"ISO8859-1" \
	    env LC_ALL=en_US.ISO8859-1 $(atf_get_srcdir)/print_charset

	# Same with a missing directory.
	export CHARSETALIASDIR=/nonexistent
	atf_check -o match:"ISO8859-1" \
	    env LC_ALL=en_US.ISO8859-1 $(atf_get_srcdir)/print_charset
}

atf_test_case map_comments
map_comments_body()
{

	cat <<EOF > charset.alias
# Easy comment

# Comment after blank line
  # Space comment

  # Space comment after blank line
	# Tabbed comment
* MAPPED
EOF

	export CHARSETALIASDIR=$PWD
	atf_check -o match:"MAPPED" $(atf_get_srcdir)/print_charset
}

atf_test_case map_growth
map_growth_body()
{

	# 64 entries is a little excessive, but it ensures that we grow the
	# map at least twice: once to get past the 8 mappings allocated in
	# .data, and and again to reallocate away from the first heap mapping.
	:> charset.alias
	for ln in $(seq 1 64); do
		echo "SRC$ln DEST$ln" >> charset.alias
	done

	export CHARSETALIASDIR=$PWD
	for ln in $(seq 1 64); do
		atf_check -o match:"DEST$ln$" env LIBCHARSET_CODESET="SRC$ln" \
		    $(atf_get_srcdir)/print_charset
	done
}

atf_test_case map_malformed
map_malformed_body()
{

	# Basic malformation: earlier entries should still map.
	cat <<EOF > charset.alias
TEST MAPPED
*
EOF

	export CHARSETALIASDIR=$PWD
	atf_check -o match:"MAPPED" \
	    env LIBCHARSET_CODESET=TEST $(atf_get_srcdir)/print_charset
}

atf_test_case map_nulbytes
map_nulbytes_body()
{

	# nul byte in the from codeset is invalid
	printf "EARLY1 MAPPED \x00 MAPPED LATE1 MAPPED * WILDCARD" > charset.alias

	# Make sure we didn't get truncated at the nul byte...
	atf_check grep -q 'WILDCARD' charset.alias

	export CHARSETALIASDIR=$PWD
	atf_check -o match:"MAPPED" \
	    env LIBCHARSET_CODESET=EARLY1 $(atf_get_srcdir)/print_charset

	# Only entries prior to the broken from codeset should appear; these
	# ones would match either of the later two entrires, but instead we'll
	# see that they remain unmapped.
	atf_check -o match:"LATE1" \
	    env LIBCHARSET_CODESET=LATE1 $(atf_get_srcdir)/print_charset
	atf_check -o match:"INCONCEIVABLE" \
	    env LIBCHARSET_CODESET=INCONCEIVABLE $(atf_get_srcdir)/print_charset

	# nul byte in the to codeset just defaults it to ASCII (libcharset default)
	printf "EARLY1 MAPPEDE DFLT \x00 LATE1 MAPPEDL * WILDCARD" > charset.alias
	atf_check grep -q 'WILDCARD' charset.alias
	atf_check -o match:"MAPPEDE" \
	    env LIBCHARSET_CODESET=EARLY1 $(atf_get_srcdir)/print_charset
	atf_check -o match:"ASCII" \
	    env LIBCHARSET_CODESET=DFLT $(atf_get_srcdir)/print_charset
	atf_check -o match:"MAPPEDL" \
	    env LIBCHARSET_CODESET=LATE1 $(atf_get_srcdir)/print_charset
	atf_check -o match:"WILDCARD" \
	    env LIBCHARSET_CODESET=INCONCEIVABLE $(atf_get_srcdir)/print_charset
}

atf_test_case map_order
map_order_body()
{

	cat <<EOF > charset.alias
TEST MAPPED1
TEST MAPPED2
* MAPPED3
EOF

	export CHARSETALIASDIR=$PWD
	atf_check -o match:"MAPPED1" \
	    env LIBCHARSET_CODESET=TEST $(atf_get_srcdir)/print_charset
}

atf_test_case static
static_body()
{

	# We'll use the built-in mapping first, so we should never be able to
	# see a version that maps to UTF-9.
	echo "* UTF-9" > charset.alias

	atf_check -o not-match:"UTF-9" \
	    $(atf_get_srcdir)/print_charset "" "$PWD"
	atf_check -o not-match:"UTF-8" \
	    $(atf_get_srcdir)/print_charset "$PWD" ""
}

atf_test_case whitespace
whitespace_body()
{

	# Just add a lot of leading whitespace and some interior whitespace...
	cat <<EOF > charset.alias
	  TEST	     MAPPED
* WILDCARD
EOF

	export CHARSETALIASDIR=$PWD
	atf_check -o match:"MAPPED" \
	    env LIBCHARSET_CODESET=TEST $(atf_get_srcdir)/print_charset
	atf_check -o match:"WILDCARD" \
	    env LIBCHARSET_CODESET=OTEST $(atf_get_srcdir)/print_charset
}

atf_init_test_cases()
{

	atf_add_test_case builtin
	atf_add_test_case csalias
	atf_add_test_case empty
	atf_add_test_case map_comments
	atf_add_test_case map_growth
	atf_add_test_case map_malformed
	atf_add_test_case map_nulbytes
	atf_add_test_case map_order
	atf_add_test_case static
	atf_add_test_case whitespace
}
