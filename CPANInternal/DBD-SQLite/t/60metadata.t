use Test;
BEGIN { plan tests => 27 }
use DBI;
my $dbh = DBI->connect("dbi:SQLite:dbname=foo", "", "", { });
ok($dbh);
$dbh->{PrintError} = 0;
$dbh->do("drop table meta$_") for 1..5;
$dbh->{PrintError} = 1;
ok $dbh->do("create table meta1 (f1 varchar(2) PRIMARY KEY, f2 char(1))");
ok $dbh->do("create table meta2 (f1 varchar(2), f2 char(1), PRIMARY KEY (f1))");
ok $dbh->do("create table meta3 (f2 char(1), f1 varchar(2) PRIMARY KEY)");
$dbh->trace(0);
$DBI::neat_maxlen = 4000;
my $sth = $dbh->primary_key_info('', '', '%');
ok $sth;
my $pki = $sth->fetchall_hashref('TABLE_NAME');
ok $pki;
#use Data::Dumper; print Dumper($pki);
ok keys %$pki == 3;
ok $_->{COLUMN_NAME} eq 'f1' for values %$pki;

ok $dbh->do("create table meta4 (f1 varchar(2), f2 char(1), PRIMARY KEY (f1,f2))");
$sth = $dbh->primary_key_info('', '', 'meta4');
ok $sth;
$pki = $sth->fetchall_hashref('COLUMN_NAME');
ok $pki;
#use Data::Dumper; print Dumper($pki);
ok keys %$pki == 2;
ok $pki->{f1}->{KEY_SEQ} == 1;
ok $pki->{f2}->{KEY_SEQ} == 2;

my @pk = $dbh->primary_key('','','meta4');
ok @pk == 2;
ok "@pk" eq "f1 f2";

ok $dbh->do("insert into meta4 values ('xyz', 'b')");
$sth = $dbh->prepare("select * from meta4");
ok $sth;
ok $sth->execute();
ok $sth->fetch();
my $types = $sth->{TYPE};
my $names = $sth->{NAME};
# warn("Types: @$types, Names: @$names\n");
ok( @$types == @$names );
print "# Types: @$types\n";
print "# Names: @$names\n";
ok($types->[0] eq 'varchar(2)');
ok($types->[1] eq 'char(1)');

ok $dbh->do("create table meta5 ( f1 integer PRIMARY KEY )");
@pk = $dbh->primary_key(undef, undef, 'meta5');
ok($pk[0] eq 'f1');
