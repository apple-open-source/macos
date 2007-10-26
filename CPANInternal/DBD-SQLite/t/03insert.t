use Test;
use DBI;
BEGIN { plan tests => 10 }
my $dbh = DBI->connect("dbi:SQLite:dbname=foo", "", "");
ok($dbh);
my $sth = $dbh->prepare("INSERT INTO f VALUES (?, ?, ?)");
ok($sth);
ok(my $rows = $sth->execute("Fred", "Bloggs", "fred\@bloggs.com"));
ok($rows == 1);
ok($dbh->func('last_insert_rowid'));
my $unless_min_dbi =
    $DBI::VERSION < 1.43 ? 'last_insert_id requires DBI v1.43' : '';
skip($unless_min_dbi, $dbh->last_insert_id(undef, undef, undef, undef) );
ok($sth->execute("test", "test", "1"));
ok($sth->execute("test", "test", "2"));
ok($sth->execute("test", "test", "3"));
ok($dbh->do("delete from f where f1='test'") == 3);
$sth->finish;
undef $sth;
$dbh->disconnect;
