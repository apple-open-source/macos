#!/usr/bin/perl

# This is testing the transaction support.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 28;
# use Test::NoWarnings;

my $warning_count = 0;




#####################################################################
# Support functions

sub insert {
	Test::More::ok(
		$_[0]->do("INSERT INTO one VALUES (1, 'Jochen')"),
		'INSERT 1',
	);
}

sub rows {
	my $dbh      = shift;
	my $expected = shift;
	is_deeply(
		$dbh->selectall_arrayref('select count(*) from one'),
		[ [ $expected ] ],
		"Found $expected rows",
	);
}





#####################################################################
# Main Tests

# Create a database
my $dbh = connect_ok( dbfile => 'foo', RaiseError => 1 );

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name CHAR (64) NOT NULL
)
END_SQL

# Turn AutoCommit off
$dbh->{AutoCommit} = 0;
ok( ! $dbh->{AutoCommit}, 'AutoCommit is off' );
ok( ! $dbh->err,          '->err is false'    );
ok( ! $dbh->errstr,       '->err is false'    );

# Check rollback
insert( $dbh );
rows( $dbh, 1 );
ok( $dbh->rollback, '->rollback ok' );
rows( $dbh, 0 );

# Check commit
ok( $dbh->do('DELETE FROM one WHERE id = 1'), 'DELETE 1' );
rows( $dbh, 0 );
ok( $dbh->commit, '->commit ok' );
rows( $dbh, 0 );

# Check auto rollback after disconnect
insert( $dbh );
rows( $dbh, 1 );
ok( $dbh->disconnect, '->disconnect ok' );
$dbh = connect_ok( dbfile => 'foo' );
rows( $dbh, 0 );

# Check that AutoCommit is back on again after the reconnect
is( $dbh->{AutoCommit}, 1, 'AutoCommit is on' );

# Check whether AutoCommit mode works.
insert( $dbh );
rows( $dbh, 1 );
ok( $dbh->disconnect, '->disconnect ok' );
$dbh = connect_ok( dbfile => 'foo' );
rows( $dbh, 1 );

# Check whether commit issues a warning in AutoCommit mode
ok( $dbh->do("INSERT INTO one VALUES ( 2, 'Tim' )"), 'INSERT 2' );
SCOPE: {
	local $@ = '';
	$SIG{__WARN__} = sub {
		$warning_count++;
	};
	eval {
		$dbh->commit;
	};
	$SIG{__WARN__} = 'DEFAULT';
	is( $warning_count, 1, 'Got one warning' );
}

# Check whether rollback issues a warning in AutoCommit mode
# We accept error messages as being legal, because the DBI
# requirement of just issueing a warning seems scary.
ok( $dbh->do("INSERT INTO one VALUES ( 3, 'Alligator' )"), 'INSERT 3' );
SCOPE: {
	local $@ = '';
	$SIG{__WARN__} = sub {
		$warning_count++;
	};
	eval {
		$dbh->rollback;
	};
	$SIG{__WARN__} = 'DEFAULT';
	is( $warning_count, 2, 'Got one warning' );
}
