use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 11;

for my $name ( qw( EST MST HST CET EET MET WET EST5EDT CST6CDT MST7MDT PST8PDT ) )
{
    my $tz = eval { DateTime::TimeZone->new( name => $name ) };
    ok( $tz, "got a timezone for name => $name" );
}
