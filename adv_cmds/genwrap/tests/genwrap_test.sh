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
	atf_check -o file:$(atf_get_srcdir)/analytics_simple_g.out \
	    ${asimple} -a --count=3 -d -a --count=6 -a

	# Most of the basics checked, just run a couple of argument tests.
	atf_check -o match:"--type__00 dog" ${asimple} --type dog
	atf_check -o match:"--type dog" ${asimple} --type dog
	atf_check -o match:"-t__00 dog" ${asimple} -t dog
	atf_check -o match:"-t dog" ${asimple} -t dog

	# Also test our repetition capabilities
	atf_check -o match:"-C 1" ${asimple} -C
	atf_check -o match:"-C 23" ${asimple} -C23
	atf_check -o match:"-C 234" ${asimple} -C234
	atf_check -o match:"-C 1" ${asimple} -C2345

	# Must be a complete match, no off-by-one.
	atf_check -o match:"--type__00 1" ${asimple} --type sdog
	atf_check -o match:"--type__00 1" ${asimple} --type dogs
	atf_check -o match:"--type__00 1" ${asimple} --type sdogs

	# Multiple appearances, last one doesn't match should just set the
	# overall option to the # found.
	atf_check -o match:"--type 2" ${asimple} --type dogs --type sdogs

	# Finally, make sure we preserve multiple uses
	atf_check -o save:analytics_simple_multiarg.out \
	    ${asimple} --type dog --type fish

	atf_check -o match:"--type__00 dog" cat analytics_simple_multiarg.out
	atf_check -o match:"--type__01 fish" cat analytics_simple_multiarg.out
	atf_check -o match:"--type fish" cat analytics_simple_multiarg.out
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

atf_test_case arg_selector_simple_varsel
arg_selector_simple_varsel_body()
{
	mkdir -p bin

	printf "#!/bin/sh\necho old" > bin/foo
	printf "#!/bin/sh\necho new" > bin/newfoo

	chmod 755 bin/foo bin/newfoo

	# -a normally matches newfoo, since it's first...
	atf_check -o match:"new" $(atf_get_srcdir)/arg_selector_simple_a -a

	# ... but /var/select/arg_selector_simple_a should be able to promote
	# foo to the default.

	atf_check sudo ln -sf foo /var/select/arg_selector_simple_a
	atf_check -o match:"old" $(atf_get_srcdir)/arg_selector_simple_a -a
	atf_check sudo rm -f /var/select/arg_selector_simple_a
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

atf_test_case arg_selector_complex_logonly_args
arg_selector_complex_logonly_args_body()
{
	mkdir -p bin

	# newfoo supports the the -z flag, but -x and -y and marked logonly.
	# foo is in logonly argmode, so it should be the fallback for pretty
	# much any option not enumerated in the newfoo set.
	printf "#!/bin/sh\necho old" > bin/foo
	printf "#!/bin/sh\necho new" > bin/newfoo

	chmod 755 bin/foo bin/newfoo

	# No args and -z should go to newfoo.
	atf_check -o match:"new" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args
	atf_check -o match:"new" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args -z

	# -n should trigger a fallback to foo, along with -x and -y.
	atf_check -o match:"old" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args -z -n
	atf_check -o match:"old" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args -n
	atf_check -o match:"old" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args -y
	# Long and short forms, to be sure.
	atf_check -o match:"old" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args --exit
	atf_check -o match:"old" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args -x
	# Valid -z on either side doesn't save it.
	atf_check -o match:"old" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args -z -x
	atf_check -o match:"old" \
	    $(atf_get_srcdir)/arg_selector_complex_logonly_args -x -z
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

	# foo is the default, but make sure we can select "newfoo" with
	# /var/select/env_selector
	atf_check sudo ln -sf newfoo /var/select/env_selector
	atf_check -o match:"new" $(atf_get_srcdir)/env_selector

	# Unknown options should just fall back; we'll use on that is overly
	# long, one that fits but isn't known.
	atf_check sudo ln -sf undefinedfoo /var/select/env_selector
	atf_check -o match:"old" $(atf_get_srcdir)/env_selector
	atf_check sudo ln -sf app /var/select/env_selector
	atf_check -o match:"old" $(atf_get_srcdir)/env_selector

	# This is more appropriate for a cleanup() routine, but we don't
	# currently run atf cleanup routines on failure.  As a result, we may
	# see some collateral damage in later tests if the above env_selector
	# invocations failed for some reason.
	atf_check sudo rm /var/select/env_selector
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

atf_test_case env_selector_varsel
env_selector_varsel_body()
{
	mkdir -p bin

	printf "#!/bin/sh\necho old" > bin/foo
	printf "#!/bin/sh\necho new" > bin/newfoo

	chmod 755 bin/foo bin/newfoo

	# foo is the default, but make sure we can select "newfoo" with
	# /var/select/env_selector
	atf_check sudo ln -sf newfoo /var/select/env_selector
	atf_check -o match:"new" $(atf_get_srcdir)/env_selector

	# Unknown options should just fall back; we'll use on that is overly
	# long, one that fits but isn't known.
	atf_check sudo ln -sf undefinedfoo /var/select/env_selector
	atf_check -o match:"old" $(atf_get_srcdir)/env_selector
	atf_check sudo ln -sf app /var/select/env_selector
	atf_check -o match:"old" $(atf_get_srcdir)/env_selector

	# This is more appropriate for a cleanup() routine, but we don't
	# currently run atf cleanup routines on failure.  As a result, we may
	# see some collateral damage in later tests if the above env_selector
	# invocations failed for some reason.
	atf_check sudo rm /var/select/env_selector
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

	atf_check -o file:out.c -x "cat $spec | $GENWRAP -n simple_shim -o /dev/stdout -"
}

atf_test_case ui_outfile_stdout
ui_outfile_stdout_body()
{

	# We won't actually run this, so no need to setup bin/foo.
	spec=$(atf_get_srcdir)/simple_shim.wrapper

	atf_check $GENWRAP -o out.c $spec
	atf_check test -s out.c

	atf_check -o file:out.c -x "cat $spec | $GENWRAP -n simple_shim -o - /dev/stdin"
}

atf_init_test_cases()
{

	atf_add_test_case analytics
	atf_add_test_case arg_selector_simple
	atf_add_test_case arg_selector_simple_varsel
	atf_add_test_case arg_selector_complex
	atf_add_test_case arg_selector_complex_logonly_args
	atf_add_test_case env_selector
	atf_add_test_case env_selector_addarg
	atf_add_test_case env_selector_varsel
	atf_add_test_case simple_shim
	atf_add_test_case ui_infile_stdin
	atf_add_test_case ui_outfile_stdout
}
