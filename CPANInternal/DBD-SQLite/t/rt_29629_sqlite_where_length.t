#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 17;
use Test::NoWarnings;
use DBI qw(:sql_types);

my $dbh = connect_ok();

$dbh->do('drop table if exists artist');
$dbh->do(<<'END_SQL');
create table artist (
  id int not null primary key,
  name text not null
)
END_SQL

ok( $dbh->do(q/insert into artist (id,name) values(1, 'Leonardo da Vinci')/), 'insert');

# length works in a select list...
my $sth = $dbh->prepare('select length(name) from artist where id=?');
ok( $sth->execute(1), 'execute, select length' );
is( $sth->fetchrow_arrayref->[0], 17, 'select length result' );

# but not in a where clause...
my $statement = 'select count(*) from artist where length(name) > ?';

# ...not with bind args
$sth = $dbh->prepare($statement);
ok( $sth->execute(2), "execute: $statement : [2]" );
TODO: {
	local $TODO = 'This test is currently broken again. Wait for a better fix, or use known workarounds.';
	is( $sth->fetchrow_arrayref->[0], 1, "result of: $statement : [2]" );
}

### it does work, however, from the sqlite3 CLI...
# require Shell;
# $Shell::raw = 1;
# is( sqlite3($db, "'$statement;'"), "1\n", 'sqlite3 CLI' );

# ...works without bind args, though!
$statement =~ s/\?/2/;
$sth = $dbh->prepare($statement);
ok( $sth->execute, "execute: $statement" );
is( $sth->fetchrow_arrayref->[0], 1, "result of: $statement" );

# (Jess Robinson discovered that it passes with an arg of 1)
$statement =~ s/2/1/;
$sth = $dbh->prepare($statement);
ok( $sth->execute, "execute: $statement" );
is( $sth->fetchrow_arrayref->[0], 1, "result of: $statement" );

# (...but still not with bind args)
$statement =~ s/1/?/;
$sth = $dbh->prepare($statement);
ok( $sth->execute(1), "execute: $statement : [1]" );
TODO: {
	local $TODO = 'This test is currently broken again. Wait for a better fix, or use known workarounds.';
	is( $sth->fetchrow_arrayref->[0], 1, "result of: $statement [1]" );
}

# known workarounds 1: use bind_param explicitly

$sth = $dbh->prepare($statement);
$sth->bind_param(1, 2, { TYPE => SQL_INTEGER });
ok( $sth->execute, "execute: $statement : [2]" );
is( $sth->fetchrow_arrayref->[0], 1, "result of: $statement : [2]" );

# known workarounds 2: add "+0" to let sqlite convert the binded param into number

$statement =~ s/\?/\?\+0/;
$sth = $dbh->prepare($statement);
ok( $sth->execute(2), "execute: $statement : [2]" );
is( $sth->fetchrow_arrayref->[0], 1, "result of: $statement : [2]" );

