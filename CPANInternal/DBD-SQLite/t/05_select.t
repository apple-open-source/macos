#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 22;
use Test::NoWarnings;

my $dbh = connect_ok( RaiseError => 1 );
$dbh->do("CREATE TABLE f (f1, f2, f3)");
my $sth = $dbh->prepare("INSERT INTO f VALUES (?, ?, ?)", { go_last_insert_id_args => [undef, undef, undef, undef] });
$sth->execute("Fred", "Bloggs", "fred\@bloggs.com");

$sth = $dbh->prepare("SELECT * FROM f");
ok($sth);
ok($sth->execute);
my $row = $sth->fetch;
ok($row);
is(@$row, 3);
my $rows = $sth->execute;
ok($rows);
ok($sth->fetch);
$sth->finish;
$sth = $dbh->prepare("INSERT INTO f (f1, f2, f3) VALUES (?, ?, ?)");
ok($sth);
ok($sth->execute("test", "test", 1));
$sth->finish;
$sth = $dbh->prepare("DELETE FROM f WHERE f3 = ?");
ok($sth);
ok($sth->execute("1"));
$sth->finish;
$sth = $dbh->prepare("SELECT * FROM f");
ok($sth);
ok($sth->execute());
my $num_rows = 0;
while ($row = $sth->fetch) {
	$num_rows++;
}	
is($num_rows, 1, "Check num_rows ($num_rows) == 1");
$sth->finish;
$dbh->do("delete from f where f1='test'");
$sth = $dbh->prepare("INSERT INTO f (f1, f2, f3) VALUES (?, ?, ?)");
ok($sth);
ok($sth->execute("test", "test", 1.05));
$sth = $dbh->prepare("DELETE FROM f WHERE f3 = ?");
ok($sth);
ok($sth->execute("1.05"));
$sth->finish;
$sth = $dbh->prepare("SELECT * FROM f");
ok($sth);
ok($sth->execute());
$num_rows = 0;
while ($row = $sth->fetch) {
	$num_rows++;
}	
ok($num_rows == 1);
$sth->finish;
$dbh->do("delete from f where f1='test'");
