#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test qw/connect_ok/;
use Test::More;
use Test::NoWarnings;

plan tests => 8;

my $dbh = connect_ok( RaiseError => 1 );

eval { $dbh->do("\n") };
ok !$@, "empty statement does not spit a warning";
diag $@ if $@;

eval { $dbh->do("     ") };
ok !$@, "empty statement does not spit a warning";
diag $@ if $@;

eval { $dbh->do("") };
ok !$@, "empty statement does not spit a warning";
diag $@ if $@;

eval { $dbh->do("/* everything in a comment */") };
ok !$@, "empty statement does not spit a warning";
diag $@ if $@;

eval { $dbh->do("-- everything in a comment") };
ok !$@, "empty statement does not spit a warning";
diag $@ if $@;

eval { $dbh->do(undef) };
ok !$@, "undef statement does not spit a warning, and does not die anyway";
diag $@ if $@;
