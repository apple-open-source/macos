#!/usr/bin/perl -w

use strict;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 2;

use DateTime::TimeZone;

my $tz = DateTime::TimeZone->new( name => 'Libya' );
is( $tz->name, 'Africa/Tripoli', 'check ->name' );

$tz = DateTime::TimeZone->new( name => 'US/Central' );
is( $tz->name, 'America/Chicago', 'check ->name' );
