#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 33;
use Test::NoWarnings;
use DBI ':sql_types';

# Create a database
my $dbh = connect_ok( dbfile => 'foo', RaiseError => 1, PrintError => 1, PrintWarn => 1 );

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name CHAR (64) NULL
)
END_SQL

my $konig = "Andreas K\xf6nig";

SCOPE: {
	my $sth = $dbh->prepare("INSERT INTO one VALUES ( ?, ? )");
	isa_ok( $sth, 'DBI::st' );
	
	# Automatic type detection
	my $number = 1;
	my $char   = "A";
	ok( $sth->execute($number, $char), 'EXECUTE 1' );

	# Does the driver remember the automatically detected type?
	ok( $sth->execute("3", "Jochen Wiedmann"), 'EXECUTE 2' );
	$number = 2;
	$char   = "Tim Bunce";
	ok( $sth->execute($number, $char), 'EXECUTE 3');

	# Now try the explicit type settings
	ok( $sth->bind_param(1, " 4", SQL_INTEGER), 'bind 1' );
	ok( $sth->bind_param(2, $konig), 'bind 2' );
	ok( $sth->execute, '->execute' );

	# Works undef -> NULL?
	ok( $sth->bind_param(1, 5, SQL_INTEGER), 'bind 3' );
	ok( $sth->bind_param(2, undef), 'bind 4' );
	ok( $sth->execute, '->execute' );
}

# Reconnect
ok( $dbh->disconnect, '->disconnect' );
$dbh = connect_ok( dbfile => 'foo' );
SCOPE: {
	my $sth = $dbh->prepare("SELECT * FROM one ORDER BY id");
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	my $id   = undef;
	my $name = undef;
	ok( $sth->bind_columns(undef, \$id, \$name), '->bind_columns' );
	ok( $sth->fetch, '->fetch' );
	is( $id,   1,   'id = 1'   );
	is( $name, 'A', 'name = A' );
	ok( $sth->fetch, '->fetch' );
	is( $id,   2,   'id = 2'   );
	is( $name, 'Tim Bunce', 'name = Tim Bunce' );
	ok( $sth->fetch, '->fetch' );
	is( $id,   3,   'id = 3'   );
	is( $name, 'Jochen Wiedmann', 'name = Jochen Wiedmann' );
	ok( $sth->fetch, '->fetch' );
	is( $id,   4,   'id = 4'   );
	is( $name, $konig, 'name = $konig' );
	ok( $sth->fetch, '->fetch' );
	is( $id,   5,   'id = 5'   );
	is( $name, undef, 'name = undef' );
}
