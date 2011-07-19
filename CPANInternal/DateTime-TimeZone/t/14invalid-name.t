use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 2;

{
    my $tz = eval { DateTime::TimeZone->new( name => 'America/Chicago; print "hello, world\n";' ) };
    like( $@, qr/invalid name/, 'make sure potentially malicious code cannot sneak into eval' );
}

{
    my $tz = eval { DateTime::TimeZone->new( name => 'Bad/Name' ) };
    like( $@, qr/invalid name/, 'make sure bad names are reported' );
}
