#!/usr/bin/perl

# Tests simple table creation

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 7;
use Test::NoWarnings;

my $dbh = connect_ok();
$dbh->do(<<'END_SQL');
CREATE TABLE f
(
f1 integer NOT NULL PRIMARY KEY,
f2 integer,
f3 text
)
END_SQL

# Confirm fix for #34408: Primary key name wrong with newline in CREATE TABLE
my $pkh = $dbh->primary_key_info( undef, undef, 'f' );
my @pk  = $pkh->fetchall_arrayref();
is_deeply( \@pk, [ [ [ undef, 'main', 'f', 'f1', 1, 'PRIMARY KEY' ] ] ], '->primary_key_info ok' );

my $sth = $dbh->prepare("SELECT f.f1, f.* FROM f");
isa_ok( $sth, 'DBI::st' );
ok( $sth->execute, '->execute ok' );
my $names = $sth->{NAME};
is( scalar(@$names), 4, 'Got 4 columns' );
is_deeply( $names, [ 'f1', 'f1', 'f2', 'f3' ], 'Table prepending is disabled by default' );

