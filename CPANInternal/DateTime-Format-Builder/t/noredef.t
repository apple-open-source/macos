# $Id: noredef.t,v 1.3 2003/06/24 07:16:28 koschei Exp $
use lib 'inc';
use blib;
use strict;
use Test::More tests => 2;
use vars qw( $class %parsers );

BEGIN {
    $class = 'DateTime::Format::Builder';
    use_ok $class;
}

%parsers = (
    parsers => {
	parse_datetime =>
	{
	    length => 8,
	    regex => qr/^abcdef$/,
	    params => [qw( year month day )],
	}
    }
);

# Verify method (non-)creation

# Ensure we don't have people wiping out their other methods
{
    my $class = 'SampleClassHasParser';
    sub SampleClassHasParser::parse_datetime { return "4" }
    eval q[
	package SampleClassHasParser;
	use DateTime::Format::Builder
	    constructor => 1,
	    %::parsers;
	1;
    ];
    ok( $@, "Error when creating class." );
}
