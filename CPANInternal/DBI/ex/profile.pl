#!/usr/bin/perl -w

use DBI;

$dbh = DBI->connect('dbi:SQLite:dbname=ex_profile.db', '', '', { RaiseError => 1 });

$dbh->do("DROP TABLE IF EXISTS ex_profile");
$dbh->do("CREATE TABLE ex_profile (a int)");

 $dbh->do("INSERT INTO ex_profile (a) VALUES ($_)", undef)     for 1..100;
#$dbh->do("INSERT INTO ex_profile (a) VALUES (?)",  undef, $_) for 1..100;

my $select_sql = "SELECT a FROM ex_profile";

$dbh->selectall_arrayref($select_sql);

$dbh->selectall_hashref($select_sql, 'a');

my $sth = $dbh->prepare($select_sql);
$sth->execute;
while ( @row = $sth->fetchrow_array ) {
}


__DATA__
