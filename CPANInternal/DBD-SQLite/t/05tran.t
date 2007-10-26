use Test;
BEGIN { plan tests => 2 }
use DBI;
unlink("foo");
my $dbh = DBI->connect("dbi:SQLite:dbname=foo", "", "",
  {AutoCommit => 0, RaiseError => 1});

# $dbh->trace(2);
ok($dbh);

$dbh->do("CREATE TABLE MST (id, lbl)");
$dbh->do("CREATE TABLE TRN (no, id, qty)");

$dbh->commit; #not work?
$dbh->do("INSERT INTO MST VALUES(1, 'ITEM1')");
$dbh->do("INSERT INTO MST VALUES(2, 'ITEM2')");
$dbh->do("INSERT INTO MST VALUES(3, 'ITEM3')");
$dbh->do("INSERT INTO TRN VALUES('A', 1, 5)");
$dbh->do("INSERT INTO TRN VALUES('B', 2, 2)");
$dbh->do("INSERT INTO TRN VALUES('C', 1, 4)");
$dbh->do("INSERT INTO TRN VALUES('D', 3, 3)");
$dbh->rollback; #not work?

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
undef $sth;
$dbh->disconnect;
