#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 3;
use Test::NoWarnings;

my $dbh = connect_ok(
	AutoCommit => 0,
	RaiseError => 1,
);

$dbh->do("CREATE TABLE MST (id, lbl)");
$dbh->do("CREATE TABLE TRN (no, id, qty)");

$dbh->commit;
$dbh->do("INSERT INTO MST VALUES(1, 'ITEM1')");
$dbh->do("INSERT INTO MST VALUES(2, 'ITEM2')");
$dbh->do("INSERT INTO MST VALUES(3, 'ITEM3')");
$dbh->do("INSERT INTO TRN VALUES('A', 1, 5)");
$dbh->do("INSERT INTO TRN VALUES('B', 2, 2)");
$dbh->do("INSERT INTO TRN VALUES('C', 1, 4)");
$dbh->do("INSERT INTO TRN VALUES('D', 3, 3)");
$dbh->rollback;

my $sth = $dbh->prepare(
"SELECT TRN.id AS ID, MST.LBL AS TITLE,
        SUM(qty) AS TOTAL FROM TRN,MST
WHERE TRN.ID = MST.ID
GROUP BY TRN.ID ORDER BY TRN.ID DESC");
my $rows = $sth->execute();
ok($rows, "0E0");
my $names = $sth->{NAME};
print(join(', ', @$names), "\n");
while(my $raD = $sth->fetchrow_arrayref()) {
	print join(":", @$raD), "\n";
}

$dbh->rollback;
