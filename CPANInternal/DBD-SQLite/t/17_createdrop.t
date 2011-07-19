#!/usr/bin/perl

# This is a skeleton test. For writing new tests, take this file
# and modify/extend it.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 4;
use Test::NoWarnings;

# Create a database
my $dbh = connect_ok();

# Create a table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name CHAR (64) NOT NULL
)
END_SQL

# Drop the table
ok( $dbh->do('DROP TABLE one'), 'DROP TABLE' );
