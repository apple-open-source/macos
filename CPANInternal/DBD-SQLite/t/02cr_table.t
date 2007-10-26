$|++;
use strict;
use Test;
BEGIN { plan tests => 4 }
use DBI;
unlink("foo");
my $dbh = DBI->connect("dbi:SQLite:dbname=foo", "", "");
ok($dbh);
$dbh->{AutoCommit} = 1;
$dbh->do("CREATE TABLE f (f1, f2, f3)");
my $sth = $dbh->prepare("SELECT f.f1, f.* FROM f");
ok($sth->execute());
my $names = $sth->{NAME};
ok(@$names == 4);
print("# ", join(', ', @$names), "\n");
ok($names->[0] eq "f1");	# make sure the "f." is removed
undef $sth;
$dbh->disconnect;
