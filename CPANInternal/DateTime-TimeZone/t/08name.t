use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 4;

{
    my $tz = DateTime::TimeZone->new( name => '-0300' );
    is( $tz->name, '-0300', 'name should match value given in constructor' );
}

{
    my $tz = DateTime::TimeZone->new( name => 'floating' );
    is( $tz->name, 'floating', 'name should match value given in constructor' );
}

{
    my $tz = DateTime::TimeZone->new( name => 'America/Chicago' );
    is( $tz->name, 'America/Chicago', 'name should match value given in constructor' );
}

{
    my $tz = DateTime::TimeZone->new( name => 'UTC' );
    is( $tz->name, 'UTC', 'name should match value given in constructor' );
}
