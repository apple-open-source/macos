use strict;

use Test::More tests => 1;

use DateTime::Format::Builder;


my %parsers = (
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
	    %parsers;
	1;
    ];
    ok( $@, "Error when creating class." );
}
