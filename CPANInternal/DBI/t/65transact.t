#!perl -w
$|=1;

use strict;

use DBI;

use Test::More;

plan skip_all => 'Transactions not supported by DBD::Gofer'
    if $ENV{DBI_AUTOPROXY} && $ENV{DBI_AUTOPROXY} =~ /^dbi:Gofer/i;

plan tests => 10;

my $dbh = DBI->connect('dbi:ExampleP(AutoCommit=>1):', undef, undef)
    or die "Unable to connect to ExampleP driver: $DBI::errstr";

print "begin_work...\n";
ok($dbh->{AutoCommit});
ok(!$dbh->{BegunWork});

ok($dbh->begin_work);
ok(!$dbh->{AutoCommit});
ok($dbh->{BegunWork});

$dbh->commit;
ok($dbh->{AutoCommit});
ok(!$dbh->{BegunWork});

ok($dbh->begin_work({}));
$dbh->rollback;
ok($dbh->{AutoCommit});
ok(!$dbh->{BegunWork});

1;
