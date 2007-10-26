#!/usr/bin/perl -w

use strict;

use lib './t';

$ENV{PERL_NO_VALIDATION} = 0;
require Params::Validate;
Params::Validate->import(':all');

use vars qw(@expect);
@expect = (
	   # mandatory/optional
	   0,
	   q|^Mandatory parameter 'bar' missing|,
	   q|^Mandatory parameters .* missing|,
	   q|^The following parameter .* baz|,
	   0,
	   0,
	   0,
	   0,
	   0,

	   # simple types
	   q|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to main::sub3 was an 'arrayref'.* types: scalar|,
	   q|^The 'brax' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to main::sub3 was an 'arrayref'.* types: scalar hash|,
	   q|^The 'bar' parameter \("HASH\(0x[a-f0-9]+\)"\) to main::sub3 was a 'hashref'.* types: arrayref|,
	   0,

	   # funkier types
	   q|^The 'bar' parameter \("GLOB\(0x[a-f0-9]+\)"\) to main::sub4 was a 'globref'.* types: glob|,
	   q|^The 'baz' parameter \((?:"\*main::FH"\|GLOB)\) to main::sub4 was a 'glob'.* types: globref|,
	   q|^The 'foo' parameter \("foobar"\) to main::sub4 was a 'scalar'.* types: scalarref|,
	   q|^The 'quux' parameter \("GLOB\(0x[a-f0-9]+\)"\) to main::sub4 was a 'globref'.* types: coderef|,

	   0,
	   0,
	   q|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to main::sub4a was an 'arrayref'.* types: glob globref|,

	   0,
	   0,

	   # wasa
	   0,
	   0,
	   0,

	   q|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to main::sub6 was not a 'Bar'|,
	   0,
	   0,

	   q|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to main::sub7 was not a 'Baz'|,
	   q|^The 'foo' parameter \("Bar=SCALAR\(0x[a-f0-9]+\)"\) to main::sub7 was not a 'Baz'|,
	   0,

	   q|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to main::sub8 was not a 'Yadda'|,
	   0,

	   # can
	   0,
	   0,
	   q|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to main::sub9a does not have the method: 'barify'|,
	   0,
	   q|^The 'foo' parameter \("Baz=SCALAR\(0x[a-f0-9]+\)"\) to main::sub9b does not have the method: 'yaddaify'|,
	   q|^The 'foo' parameter \("Quux=SCALAR\(0x[a-f0-9]+\)"\) to main::sub9b does not have the method: 'barify'|,
	   q|^The 'foo' parameter \("Bar=SCALAR\(0x[a-f0-9]+\)"\) to main::sub9c does not have the method: 'yaddaify'|,
	   0,

	   # callbacks
	   0,
	   0,
	   q|^The 'foo' parameter \("20"\) to main::sub10 did not pass the 'less than 20' callback|,

	   0,
	   q|^The 'foo' parameter \("20"\) to main::sub11 did not pass the 'less than 20' callback|,
	   q|^The 'foo' parameter \("0"\) to main::sub11 did not pass the 'more than 0' callback|,

	   # mix n' match
	   q|^The 'foo' parameter \("1"\) to main::sub12 was a 'scalar'.* types: arrayref|,
	   q|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to main::sub12 did not pass the '5 elements' callback|,
	   0,

	   # positional - 1
	   q|^1 parameter was passed to .* but 2 were expected|,
	   q|^Parameter #2 \("ARRAY\(0x[a-f0-9]+\)"\) to .* did not pass the '5 elements' callback|,

	   # positional - 2
	   q|^Parameter #3 \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to .* was not a 'Bar'|,
	   0,

	   # hashref named params
	   q|^The 'bar' parameter \("HASH\(0x[a-f0-9]+\)"\) to .* was a 'hashref'.* types: arrayref|,
	   q|^Mandatory parameter 'bar' missing|,

	   # positional - 3
	   q|^3 parameters were passed .* but 1 - 2 were expected|,
	   0,
	   0,
	   q|^0 parameters were passed .* but 1 - 2 were expected|,

	   # positional - 4
	   q|^3 parameters were passed .* but 1 - 2 were expected|,
	   0,
	   0,
	   q|^0 parameters were passed .* but 1 - 2 were expected|,

	   # positional - too few arguments supplied ##test 65 onward
	   q|^0 parameters were passed .* but 3 - 4 were expected|,
	   q|^2 parameters were passed .* but 3 - 4 were expected|,
	   q|^0 parameters were passed .* but 3 - 4 were expected|,
	   q|^2 parameters were passed .* but 3 - 4 were expected|,

	   # validation_options
	   0,
	   q|^The following parameter .* FOO|,

	   0,
	   q|^The following parameter .* -foo|,

	   0,
	   q|^The following parameter .* bar|,

	   '',
	   q|^2 parameters were passed .* but 1.*|,
	   q|^Mandatory parameter 'foo' missing|,

	   q|^ERROR WAS: The following parameter .* bar|,
	   q|^The following parameter .* bar|,

	   q|^The 'foo' parameter \(undef\) to .* was an 'undef'.*|,
	   0,

	   q|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to .* was an 'arrayref'.*|,
	   0,

           0,
           q|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to .* was an 'arrayref'.*|,
           0,

           0,

           0,
           q|^Parameter #1 \("1 element"\) to .* was a 'scalar'.*|,
           0,

           q|^Odd number|,

           # optional glob
           0,
	  );

# 5.6.0 core dumps all over during the tests
if ( $] == 5.006 )
{
    warn <<'EOF';

Skipping tests for Perl 5.6.0.  5.6.0 core dumps all over during the
tests.  This may just have to do with the test code rather than the
module itself.  5.6.1 works fine when I tested it.  5.6.0 is buggy.
You are encouraged to upgrade.
EOF

    print "1..0";
    exit;
}

require 'tests.pl';

run_tests();
