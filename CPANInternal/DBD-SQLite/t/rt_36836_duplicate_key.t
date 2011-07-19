#!/usr/bin/perl

# This is a simple insert/fetch test.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 5;
use Test::NoWarnings;

# Create a database
my $dbh = connect_ok( PrintError => 0 );

# Create a database
ok( $dbh->do('CREATE TABLE one ( num INTEGER UNIQUE)'), 'create table' );

# Insert a row into the test table
ok( $dbh->do('INSERT INTO one ( num ) values ( 1 )'), 'insert' );

# Insert a duplicate
ok( ! $dbh->do('INSERT INTO one ( num ) values ( 1 )'), 'duplicate' );
