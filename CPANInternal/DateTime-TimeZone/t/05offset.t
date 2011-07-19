use strict;
use warnings;

use DateTime::TimeZone;

use Test::More tests => 32;

is( DateTime::TimeZone::offset_as_string(0), "+0000",
    "offset_as_string does the right thing on 0" );
is( DateTime::TimeZone::offset_as_string(3600), "+0100",
    "offset_as_string works on positive whole hours" );
is( DateTime::TimeZone::offset_as_string(-3600), "-0100",
    "offset_as_string works on negative whole hours" );
is( DateTime::TimeZone::offset_as_string(5400), "+0130",
    "offset_as_string works on positive half hours" );
is( DateTime::TimeZone::offset_as_string(-5400), "-0130",
    "offset_as_string works on negative half hours" );
is( DateTime::TimeZone::offset_as_string(20700), "+0545",
    "offset_as_string works on positive 15min zones" );
is( DateTime::TimeZone::offset_as_string(-20700), "-0545",
    "offset_as_string works on negative 15min zones" );
is( DateTime::TimeZone::offset_as_string(359999), "+995959",
    "offset_as_string max value" );
is( DateTime::TimeZone::offset_as_string(-359999), "-995959",
    "offset_as_string min value" );
is( DateTime::TimeZone::offset_as_string(360000), undef,
    "offset_as_string exceeded max value" );
is( DateTime::TimeZone::offset_as_string(-360000), undef,
    "offset_as_string exceeded min value" );

my @offset_seconds = qw(
    0
    3600
    -3600
    5400
    -5400
    20700
    -20700
    359999
    -359999
);

my @offset_strings = qw(
    +0100
    -0100
    +0130
    -0130
    +0545
    -0545
    +995959
    -995959
);

foreach ( @offset_seconds ) {
    is( DateTime::TimeZone::offset_as_seconds(
            DateTime::TimeZone::offset_as_string( $_ )
        ),
        $_, "n -> offset_as_string -> offset_as_seconds = n "
    );
}

foreach ( @offset_strings ) {
    is( DateTime::TimeZone::offset_as_string(
            DateTime::TimeZone::offset_as_seconds( $_ )
        ),
        $_, "n -> offset_as_seconds -> offset_as_string= n "
    );
}

# just checking that calling these as class methods works
is( DateTime::TimeZone->offset_as_string(3600), '+0100',
    'offset_as_string as class method' );

is( DateTime::TimeZone->offset_as_seconds('+0100'), 3600,
    'offset_as_seconds as class method' );

{
    DateTime::TimeZone::offset_as_string(3600);

    is( $@, '',
        'calling offset_as_string does not leave $@ set' );
}

{
    DateTime::TimeZone::offset_as_seconds("+0100");

    is( $@, '',
        'calling offset_as_string does not leave $@ set' );
}
