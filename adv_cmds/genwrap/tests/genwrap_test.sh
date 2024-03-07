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

GENWRAP=/usr/local/bin/genwrap

atf_test_case analytics
analytics_body()
{
	asimple=$(atf_get_srcdir)/analytics_simple
	aredacted=$(atf_get_srcdir)/analytics_redacted

	atf_check -o file:$(atf_get_srcdir)/analytics_simple_a.out \
	    ${asimple} -a
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_b.out \
	    ${asimple} -ad
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_c.out \
	    ${asimple} -a -d
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_d.out \
	    ${asimple} -ab3
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_e.out \
	    ${asimple} -a --count=3
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_f.out \
	    ${asimple} -a --count=3 arg

	# Now try with *REDACTED*
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_a.out \
	    ${aredacted} -a
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_b.out \
	    ${aredacted} -ad
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_c.out \
	    ${aredacted} -a -d
	atf_check -o file:$(atf_get_srcdir)/analytics_redacted_a.out \
	    ${aredacted} -ab3
	atf_check -o file:$(atf_get_srcdir)/analytics_redacted_b.out \
	    ${aredacted} -a --count=3
	atf_check -o file:$(atf_get_srcdir)/analytics_redacted_c.out \
	    ${aredacted} -a --count=3 arg
	atf_check -o file:$(atf_get_srcdir)/analytics_redacted_d.out \
	    ${aredacted} -a --count 3

	atf_check -o file:$(atf_get_srcdir)/analytics_redacted_e.out \
	    ${aredacted} -ax -y 3 -z
}

atf_test_case arg_selector_simple
arg_selector_simple_body()
{
	mkdir -p bin

	printf "#!/bin/sh\necho old" > bin/foo
	printf "#!/bin/sh\necho new" > bin/newfoo

	chmod 755 bin/foo bin/newfoo

	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_a
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_a -a
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_a -b
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_a --count=3

	# Omitted argument
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_simple_a --count

	# Unknown argument
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_simple_a -b -y
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_simple_a -x

	# Non-flag arguments
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_a -- -x

	# We'll try it again, but with a spec that doesn't include any long
	# flags, since those are two separate paths through the shim.

	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_b
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_b -a
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_b -b

	# Unknown arguments
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_simple_b -b -y
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_simple_b --count
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_simple_b -x

	# Non-flag arguments
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_b -- -x
}

atf_test_case arg_selector_complex
arg_selector_complex_body()
{
	mkdir -p bin

	# Try with a chain of applications instead; the first two only support
	# a subset of flags, so we should be able to see the shim progress from
	# te default bestfoo -> newfoo -> foo based on the supported flags.
	printf "#!/bin/sh\necho old" > bin/foo
	printf "#!/bin/sh\necho new" > bin/newfoo
	printf "#!/bin/sh\necho best" > bin/bestfoo

	chmod 755 bin/foo bin/newfoo bin/bestfoo

	atf_check -o match:"best" $(atf_get_srcdir)/arg_selector_complex bar
	atf_check -o match:"best" $(atf_get_srcdir)/arg_selector_complex -a
	atf_check -o match:"best" $(atf_get_srcdir)/arg_selector_complex -b

	# Fallback to newfoo
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_complex -b -e

	# Fallback to foo
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_complex --count=3

	# test that logonly works; newfoo should be marked logonly and has no
	# long arguments defined.
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_complex_logonly --count=3
}

atf_test_case env_selector
env_selector_body()
{
	mkdir -p bin

	printf "#!/bin/sh\necho old" > bin/foo
	printf "#!/bin/sh\necho new" > bin/newfoo

	chmod 755 bin/foo bin/newfoo

	atf_check -o match:"old" $(atf_get_srcdir)/env_selector
	atf_check -o match:"old" env FOO_COMMAND="foo" $(atf_get_srcdir)/env_selector
	atf_check -o match:"new" env FOO_COMMAND="newfoo" $(atf_get_srcdir)/env_selector

	# Should use the default application if the env var is set to a bogus
	# value.
	atf_check -o match:"old" env FOO_COMMAND="invalid" $(atf_get_srcdir)/env_selector
}

atf_test_case env_selector_addarg
env_selector_addarg_body()
{
	mkdir -p bin

	printf "#!/bin/sh\necho \$@" > bin/foo

	chmod 755 bin/foo

	atf_check -o match:"^$" $(atf_get_srcdir)/env_selector_addarg

	# Only adds one arg
	atf_check -o match:"-x" \
	    env FOO_COMMAND="newfoo" $(atf_get_srcdir)/env_selector_addarg
	atf_check -o match:"-x -y" \
	    env FOO_COMMAND="newfoo" $(atf_get_srcdir)/env_selector_addarg -y

	# Adds multiple args, but in the same addarg
	atf_check -o match:"-x -y" \
	    env FOO_COMMAND="bestfoo" $(atf_get_srcdir)/env_selector_addarg
	atf_check -o match:"-x -y -z" \
	    env FOO_COMMAND="bestfoo" $(atf_get_srcdir)/env_selector_addarg -z

	# Same, but in multiple addargs; they should be processed in order.
	atf_check -o match:"-x -y -z" \
	    env FOO_COMMAND="worstfoo" $(atf_get_srcdir)/env_selector_addarg
	atf_check -o match:"-x -y -z -0" \
	    env FOO_COMMAND="worstfoo" $(atf_get_srcdir)/env_selector_addarg -0
}


atf_test_case simple_shim
simple_shim_body()
{
	mkdir -p bin

	printf "#!/bin/sh\necho foo" > bin/foo
	chmod 755 bin/foo

	atf_check -o match:"foo" $(atf_get_srcdir)/simple_shim
}

atf_test_case ui_infile_stdin
ui_infile_stdin_body()
{

	# We won't actually run this, so no need to setup bin/foo.
	spec=$(atf_get_srcdir)/simple_shim.wrapper

	atf_check $GENWRAP -o out.c $spec
	atf_check test -s out.c

	atf_check -o file:out.c -x "cat $spec | $GENWRAP -o /dev/stdout -"
}

atf_test_case ui_outfile_stdout
ui_outfile_stdout_body()
{

	# We won't actually run this, so no need to setup bin/foo.
	spec=$(atf_get_srcdir)/simple_shim.wrapper

	atf_check $GENWRAP -o out.c $spec
	atf_check test -s out.c

	atf_check -o file:out.c -x "cat $spec | $GENWRAP -o - /dev/stdin"
}

atf_init_test_cases()
{

	atf_add_test_case analytics
	atf_add_test_case arg_selector_simple
	atf_add_test_case arg_selector_complex
	atf_add_test_case env_selector
	atf_add_test_case env_selector_addarg
	atf_add_test_case simple_shim
	atf_add_test_case ui_infile_stdin
	atf_add_test_case ui_outfile_stdout
}
