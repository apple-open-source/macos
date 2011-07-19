#!/usr/bin/perl -w

use strict;

use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 17);
}

use File::Temp qw/tempfile/;
my (undef, $DB) = tempfile();
my @DSN = (
	"dbi:SQLite:dbname=$DB", '', '',
	{ AutoCommit => 1, RootClass => "DBIx::ContextualFetch" });

my $dbh = DBI->connect(@DSN);

$dbh->do("CREATE TABLE foo (id INTEGER, name TEXT)");
my $insert = $dbh->prepare("INSERT INTO foo VALUES (?, ?)");
$insert->execute(1, "Fred");
$insert->execute(2, "Barney");

sub make_sth {
	my $sql = shift;
	my $sth = $dbh->prepare($sql);
	return $sth;
}

{    # fetch
	(my $sth  = make_sth("SELECT * FROM foo ORDER BY id"))->execute;
	my @got1 = $sth->fetch;
	is $got1[1], "Fred", 'fetch @';
	my $got2 = $sth->fetch;
	is $got2->[1], "Barney", 'fetch $';
}

{    # Fetch Hash
	(my $sth  = make_sth("SELECT * FROM foo ORDER BY id"))->execute;
	my %got1 = $sth->fetch_hash;
	is $got1{name}, "Fred", 'fetch_hash %';
	my $got2 = $sth->fetch_hash;
	is $got2->{name}, "Barney", 'fetch_hash $';
	my %got3 = eval { $sth->fetch_hash };
	is keys %got3, 0, "Nothing at the end";
	is $@, "", "And no error";
}

{    # fetchall @
	(my $sth = make_sth("SELECT * FROM foo ORDER BY id"))->execute;
	my @got = $sth->fetchall;
	is $got[1]->[1], "Barney", 'fetchall @';
}

{    # fetchall $
	(my $sth = make_sth("SELECT * FROM foo ORDER BY id"))->execute;
	my $got = $sth->fetchall;
	is $got->[1]->[1], "Barney", 'fetchall $';
}

{    # fetchall_hash @
	(my $sth = make_sth("SELECT * FROM foo ORDER BY id"))->execute;
	my @got = $sth->fetchall_hash;
	is $got[1]->{name}, "Barney", 'fetchall_hash @';
}

{    # fetchall_hash $
	(my $sth = make_sth("SELECT * FROM foo ORDER BY id"))->execute;
	my $got = $sth->fetchall_hash;
	is $got->[1]->{name}, "Barney", 'fetchall_hash @';
}

{    # select_row
	my $sth = make_sth("SELECT * FROM foo WHERE id = ?");
	my ($id, $name) = $sth->select_row(1);
	is $name, "Fred", "select_row";
}

{    # select_col
	my $sth   = make_sth("SELECT name FROM foo where id > ? ORDER BY id");
	my @names = $sth->select_col(0);
	is $names[1], "Barney", "select_col";
}

{    # select_val
	my $sth  = make_sth("SELECT name FROM foo where id = ?");
	my $name = $sth->select_val(1);
	is $name, "Fred", "select_val";
}

{    # Execute binding
	my $sth = make_sth("SELECT * FROM foo WHERE id > ? ORDER BY id");
	$sth->execute([0], [ \my ($id, $name) ]);
	$sth->fetch;
	is $id,   1,      "bound id 1";
	is $name, "Fred", "name = Fred";
	$sth->fetch;
	is $id,   2,        "bound id 2";
	is $name, "Barney", "name = Barney";
}

