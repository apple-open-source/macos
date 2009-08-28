#!/usr/bin/perl -w

use strict;
use Test::More tests => 2;

use Time::HiRes::Value;

my $start = Time::HiRes::Value->now();

sleep( 1 );

my $end = Time::HiRes::Value->now();

my $interval = $end - $start;

# Hard to be exact about this interval, but we expect it to be between 0.9 and
# 1.1 seconds
is( $interval > 0.9, 1 );
is( $interval < 1.1, 1 );
