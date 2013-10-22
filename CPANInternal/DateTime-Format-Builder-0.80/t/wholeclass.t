use strict;

use Test::More tests => 8;


package DateTime::Format::ICal15;
use DateTime::Format::Builder;

DateTime::Format::Builder->create_class(
    version => 4.00,
    parsers => {
	parse_datetime => [ {
	    params => [ qw( year month day hour minute second ) ],
	    regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
	} ]
    },
);

package main;

my $sample = "20030716T163245";
my $newclass = "DateTime::Format::ICal15";

my $parser = $newclass->new();
cmp_ok ( $newclass->VERSION, '==', '4.00', "Version matches");

{
    my $dt = $parser->parse_datetime( $sample );
    isa_ok( $dt => "DateTime" );
    my %methods = qw(
    hour 16 minute 32 second 45
    year 2003 month 7 day 16
    );
    while (my ($method, $expected) = each %methods)
    {
	is( $dt->$method() => $expected,
	    "\$dt->$method() == $expected" );
    }
}
