#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 37;
use Test::NoWarnings;

# Create a database
my $dbh = connect_ok( dbfile => 'foo', RaiseError => 1, PrintError => 1, PrintWarn => 1 );

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name CHAR (64)
)
END_SQL

# Test quoting
my $quoted = $dbh->quote('test1');
is( $quoted, "'test1'", '->quote(test1) ok' );

# Disconnect
ok( $dbh->disconnect, '->disconnect' );

# Reconnect
$dbh = connect_ok( dbfile => 'foo' );

# Delete the table and recreate it
ok( $dbh->do('DROP TABLE one'), 'DROP' );

# Create the table again
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NULL,
    name CHAR (64) NULL
)
END_SQL

# Insert into table
ok( $dbh->do("INSERT INTO one VALUES ( 1, 'A' )"), 'INSERT 1' );

# Delete it
ok( $dbh->do('DELETE FROM one WHERE id = 1'), 'DELETE 1' );

# When we "forget" execute, fail with error message
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one WHERE id = 1');
	isa_ok( $sth, 'DBI::st' );
	my ($pe) = $sth->{PrintError};
	$sth->{PrintError} = 0;
	my $rv = eval {
		$sth->fetchrow;
	};
	$sth->{PrintError} = $pe;
	ok( $sth->execute, '->execute' );

	# This should fail without error message: No rows returned.
	my(@row, $ref);
	SCOPE: {
		local $^W = 0;
		is( $sth->fetch, undef, '->fetch returns undef' );
	}
	ok( $sth->finish, '->finish' );
}

# This section should exercise the sth->func( '_NumRows' ) private
# method by preparing a statement, then finding the number of rows
# within it. Prior to execution, this should fail. After execution,
# the number of rows affected by the statement will be returned.
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one WHERE id = 1');
	isa_ok( $sth, 'DBI::st' );
	is( $sth->rows, -1, '->rows is negative' );
	ok( $sth->execute, '->execute ok' );
	is( $sth->rows, 0, '->rows returns 0' );
	ok( $sth->finish, '->finish' );
}

# Test whether or not a field containing a NULL is returned correctly
# as undef, or something much more bizarre
ok( $dbh->do("INSERT INTO one VALUES ( NULL, 'NULL-valued id' )"), 'INSERT 2' );
SCOPE: {
	my $sth = $dbh->prepare("SELECT id FROM one WHERE id IS NULL");
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	is_deeply(
		$sth->fetchall_arrayref,
		[ [ undef ] ],
		'NULL returned ok',
	);
	ok( $sth->finish, '->finish' );
}

# Delete the test row from the table
ok( $dbh->do("DELETE FROM one WHERE id is NULL AND name = 'NULL-valued id'"), 'DELETE' );

# Test whether or not a char field containing a blank is returned
# correctly as blank, or something much more bizarre
ok( $dbh->do("INSERT INTO one VALUES ( 2, NULL )"), 'INSERT 3' );
SCOPE: {
	my $sth = $dbh->prepare("SELECT name FROM one WHERE id = 2 AND name IS NULL");
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	is_deeply(
		$sth->fetchall_arrayref,
		[ [ undef ] ],
		'->fetchall_arrayref',
	);
	ok( $sth->finish, '->finish' );
}


# Delete the test row from the table
ok( $dbh->do('DELETE FROM ONE WHERE id = 2 AND name IS NULL'), 'DELETE' );

# Test the new funky routines to list the fields applicable to a SELECT
# statement, and not necessarily just those in a table...
SCOPE: {
	my $sth = $dbh->prepare("SELECT * FROM one");
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, 'Execute'   );
	ok( $sth->execute, 'Reexecute' );
	my @row = $sth->fetchrow_array;
	ok( $sth->finish, '->finish' );
}

# Insert some more data into the test table.........
ok( $dbh->do("INSERT INTO one VALUES( 2, 'Gary Shea' )"), 'INSERT 4' );
SCOPE: {
	my $sth = $dbh->prepare("UPDATE one SET id = 3 WHERE name = 'Gary Shea'");
	isa_ok( $sth, 'DBI::st' );
}
