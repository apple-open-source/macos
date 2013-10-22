#!/usr/bin/perl

use Time::HiRes::Value;

my $start = Time::HiRes::Value->now();

my $exitcode = system( @ARGV );

my $duration = Time::HiRes::Value->now() - $start;

printf STDERR "Took $duration seconds\n";

exit( $exitcode >> 8 );
