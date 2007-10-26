#!/usr/bin/perl -w

use strict;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 1;

{
    my $tz = eval { DateTime::TimeZone->new( name => 'America/Chicago; print "hello, world\n";' ) };
    like( $@, qr/invalid name/, 'make sure potentially malicious code cannot sneak into eval' );
}
