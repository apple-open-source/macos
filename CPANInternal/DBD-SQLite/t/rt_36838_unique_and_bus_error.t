#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 4;
use Test::NoWarnings;

my $dbh = connect_ok( RaiseError => 1, PrintError => 0 );

$dbh->do("CREATE TABLE nums (num INTEGER UNIQUE)");

ok $dbh->do("INSERT INTO nums (num) VALUES (?)", undef, 1);

eval { $dbh->do("INSERT INTO nums (num) VALUES (?)", undef, 1); };
ok $@ =~ /column num is not unique/, $@;  # should not be a bus error
