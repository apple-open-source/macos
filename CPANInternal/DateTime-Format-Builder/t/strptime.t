#!/usr/bin/perl -w
# $Id: strptime.t,v 1.5 2003/08/10 13:38:29 koschei Exp $
use lib 'inc';
use blib;
use Test::More tests => 25;
use strict;
use vars qw( $class );

BEGIN {
    $class = 'DateTime::Format::Builder';
    use_ok $class;
}

my @tests = (
	# Simple dates
	['%Y-%m-%d',	'1998-12-31'],
	['%y-%m-%d', '98-12-31'],
	['%Y years, %j days', '1998 years, 312 days'],
	['%b %d, %Y', 'Jan 24, 2003'],
	['%B %d, %Y', 'January 24, 2003'],

	# Simple times
	['%H:%M:%S', '23:45:56'],
	['%l:%M:%S %p', '12:34:56 PM'],
	
	# With Nanoseconds
	['%H:%M:%S.%N', '23:45:56.123456789'],
	['%H:%M:%S.%6N', '23:45:56.123456'],
	['%H:%M:%S.%3N', '23:45:56.123'],

	# Complex dates
	['%Y;%j = %Y-%m-%d', '2003;56 = 2003-02-25'],
	[q|%d %b '%y = %Y-%m-%d|, q|25 Feb '03 = 2003-02-25|],
);


for my $test (@tests)
{
    my ($pattern, $data) = @$test;
    my $parser = $class->create_parser( strptime => $pattern );
    my $parsed = $parser->parse( $class, $data );
    isa_ok( $parsed => 'DateTime' );
    is( $parsed->strftime($pattern) => $data, $pattern);
}
