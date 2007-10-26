#!/usr/bin/perl -w

use strict;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 2;

use Storable;

my $tz1 = DateTime::TimeZone->new( name => 'America/Chicago' );
my $frozen = Storable::nfreeze($tz1);

ok( length $frozen < 2000,
    'the serialized tz object should not be immense' );

my $tz2 = Storable::thaw($frozen);

is( $tz2->name, 'America/Chicago', 'thaw frozen time zone object' );
