use strict;
use warnings;

use Test::More tests => 2;

{
    package DateTime::TimeZone::Local::foobar;

    use DateTime::TimeZone::OffsetOnly;

    sub Methods { ( 'FromHardCoded' ) }

    sub FromHardCoded { DateTime::TimeZone::OffsetOnly->new( offset => 42*60 ) }
}


use DateTime::TimeZone::OffsetOnly;

local $^O = 'foobar';

my $tz = DateTime::TimeZone->new( name => 'local' );
isa_ok( $tz, 'DateTime::TimeZone' );
is( $tz->name(), '+2520', 'os42 returns expected time zone' );

