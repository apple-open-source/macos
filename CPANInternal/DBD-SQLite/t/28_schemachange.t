#!/usr/bin/perl

# This test works, but as far as I can tell this doesn't actually test
# the thing that the test was originally meant to test.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use Test::More tests => 9;
use t::lib::Test;

my $create1 = 'CREATE TABLE table1 (id INTEGER NOT NULL, name CHAR (64) NOT NULL)';
my $create2 = 'CREATE TABLE table2 (id INTEGER NOT NULL, name CHAR (64) NOT NULL)';
my $drop1   = 'DROP TABLE table1';
my $drop2   = 'DROP TABLE table2';

# diag("Parent connecting... ($$)\n");
SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	ok( $dbh->do($create1), $create1 );
	ok( $dbh->do($create2), $create2 );
	ok( $dbh->disconnect, '->disconnect ok' );
}

my $pid;
# diag("Forking... ($$)");
if ( not defined( $pid = fork() ) ) {
	die("fork: $!");

} elsif ( $pid == 0 ) {
	# Pause to let the parent connect
	sleep(2);

	# diag("Child starting... ($$)");
	my $dbh = DBI->connect(
		'dbi:SQLite:dbname=foo', '', ''
	) or die 'connect failed';
	$dbh->do($drop2) or die "DROP ok";
	$dbh->disconnect or die "disconnect ok";
	# diag("Child exiting... ($$)");

	exit(0);

}

SCOPE: {
	# Parent process
	my $dbh = connect_ok( dbfile => 'foo' );
	# diag("Waiting for child... ($$)");
	ok( waitpid($pid, 0) != -1, "waitpid" );

	# Make sure the child actually deleted table2
	ok( $dbh->do($drop1),   $drop1   ) or diag("Error: '$DBI::errstr'");
	ok( $dbh->do($create2), $create2 ) or diag("Error: '$DBI::errstr'");
	ok( $dbh->disconnect, '->disconnect ok' );
}
