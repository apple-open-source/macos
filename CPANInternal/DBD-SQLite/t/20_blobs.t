#!/usr/bin/perl

# This is a test for correct handling of BLOBS; namely $dbh->quote
# is expected to work correctly.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 10;
use Test::NoWarnings;
use DBI ':sql_types';

sub ShowBlob($) {
    my ($blob) = @_;
    print("showblob length: ", length($blob), "\n");
    if ($ENV{SHOW_BLOBS}) { open(OUT, ">>$ENV{SHOW_BLOBS}") }
    my $i = 0;
    while (1) {
	if (defined($blob)  &&  length($blob) > ($i*32)) {
	    $b = substr($blob, $i*32);
	} else {
	    $b = "";
            last;
	}
        if ($ENV{SHOW_BLOBS}) { printf OUT "%08lx %s\n", $i*32, unpack("H64", $b) }
        else { printf("%08lx %s\n", $i*32, unpack("H64", $b)) }
        $i++;
        last if $i == 8;
    }
    if ($ENV{SHOW_BLOBS}) { close(OUT) }
}

# Create a database
my $dbh = connect_ok();
$dbh->{sqlite_handle_binary_nulls} = 1;

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name BLOB (128) NOT NULL
)
END_SQL

# Create a blob
my $blob = '';
my $b    = '';
for ( my $j = 0;  $j < 256; $j++ ) {
	$b .= chr($j);
}
for ( my $i = 0;  $i < 128; $i++ ) {
	$blob .= $b;
}

# Insert a row into the test table
SCOPE: {
	my $sth = $dbh->prepare("INSERT INTO one VALUES ( 1, ? )");
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->bind_param(1, $blob, SQL_BLOB), '->bind_param' );
	ok( $sth->execute, '->execute' );
}

# Now, try SELECT'ing the row out.
SCOPE: {
	my $sth = $dbh->prepare("SELECT * FROM one WHERE id = 1");
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	ok(
		$sth->fetchrow_arrayref->[1] eq $blob,
		'Got the blob back ok',
	);
	ok( $sth->finish, '->finish' );
}
