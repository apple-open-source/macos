use Test;
BEGIN { plan tests => 21 }
use DBI;
my $dbh = DBI->connect("dbi:SQLite:dbname=foo", "", "", { RaiseError => 1 });
ok($dbh);
# $dbh->trace(4);
my $sth = $dbh->prepare("SELECT * FROM f");
ok($sth);
ok($sth->execute);
my $row = $sth->fetch;
ok($row);
ok(@$row, 3);
print join(", ", @$row), "\n";
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
ok($num_rows == 1, 1, "Check num_rows ($num_rows) == 1");
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
undef $sth;
$dbh->do("delete from f where f1='test'");
$dbh->disconnect;
