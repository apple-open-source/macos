#!/usr/bin/perl

# Tests that executing the same prepare_cached twice without a
# finish in between does not prevent it being automatically cleaned
# up and that it does not generate a warning.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 32;
use Test::NoWarnings;

# Create the table
SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
	create table foo (
		id integer primary key not null
	)
END_SQL
	$dbh->begin_work;
	ok( $dbh->do('insert into foo values ( 1 )'), 'insert 1' );
	ok( $dbh->do('insert into foo values ( 2 )'), 'insert 2' );
	$dbh->commit;
	$dbh->disconnect;
}

# Collect the warnings
my $c = 0;
my @w = ();
$SIG{__WARN__} = sub { $c++; push @w, [ @_ ]; return };

# Conveniences
my $sql = 'select * from foo order by id';

sub fetchrow_1 {
	my $row = $_[0]->fetchrow_arrayref;
	is_deeply( $row, [ 1 ], 'Got row 1' );
}





######################################################################
# A well-behaved non-cached statement

SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare($sql);
	}
	$dbh->disconnect;
	is( $c, 0, 'No warnings' );
}

SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare($sql);
		$sth->execute;
	}
	$dbh->disconnect;
	is( $c, 0, 'No warnings' );
}

SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare($sql);
		$sth->execute;
		fetchrow_1($sth);		
	}
	$dbh->disconnect;
	is( $c, 0, 'No warnings' );
}





######################################################################
# A badly-behaved regular statement

# Double execute, no warnings
SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare($sql);
		$sth->execute;
		fetchrow_1($sth);		
		$sth->execute;
		fetchrow_1($sth);		
	}
	$dbh->disconnect;
	is( $c, 0, 'No warnings' );
}

# We expect a warnings from this one
SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	my $sth = $dbh->prepare($sql);
	$sth->execute;
	fetchrow_1($sth);		
	$dbh->disconnect;
	is( $c, 1, 'Got a warning' );
}





######################################################################
# A well-behaved cached statement

SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare_cached($sql);
	}
	$dbh->disconnect;
	is( $c, 1, 'No warnings' );
}

SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare_cached($sql);
		$sth->execute;
		fetchrow_1($sth);		
		$sth->finish;
	}
	$dbh->disconnect;
	is( $c, 1, 'No warnings' );
}

SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare_cached($sql);
		$sth->execute;
		fetchrow_1($sth);		
		$sth->finish;
	}
	SCOPE: {
		my $sth = $dbh->prepare_cached($sql);
		$sth->execute;
		fetchrow_1($sth);		
		$sth->finish;
	}
	$dbh->disconnect;
	is( $c, 1, 'No warnings' );
}





#####################################################################
# Badly-behaved prepare_cached (but still acceptable)

SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo' );
	SCOPE: {
		my $sth = $dbh->prepare_cached($sql);
		$sth->execute;
		fetchrow_1($sth);		
		$sth->execute;
		fetchrow_1($sth);		
		$sth->finish;
	}
	$dbh->disconnect;
	is( $c, 1, 'No warnings' );
}
