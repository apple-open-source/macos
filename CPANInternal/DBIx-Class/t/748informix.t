use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_INFORMIX_${_}" } qw/DSN USER PASS/};

#warn "$dsn $user $pass";

plan skip_all => 'Set $ENV{DBICTEST_INFORMIX_DSN}, _USER and _PASS to run this test'
  unless ($dsn && $user);

my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

my $dbh = $schema->storage->dbh;

eval { $dbh->do("DROP TABLE artist") };

$dbh->do("CREATE TABLE artist (artistid SERIAL, name VARCHAR(255), charfield CHAR(10), rank INTEGER DEFAULT 13);");

my $ars = $schema->resultset('Artist');
is ( $ars->count, 0, 'No rows at first' );

# test primary key handling
my $new = $ars->create({ name => 'foo' });
ok($new->artistid, "Auto-PK worked");

# test explicit key spec
$new = $ars->create ({ name => 'bar', artistid => 66 });
is($new->artistid, 66, 'Explicit PK worked');
$new->discard_changes;
is($new->artistid, 66, 'Explicit PK assigned');

# test populate
lives_ok (sub {
  my @pop;
  for (1..2) {
    push @pop, { name => "Artist_$_" };
  }
  $ars->populate (\@pop);
});

# test populate with explicit key
lives_ok (sub {
  my @pop;
  for (1..2) {
    push @pop, { name => "Artist_expkey_$_", artistid => 100 + $_ };
  }
  $ars->populate (\@pop);
});

# count what we did so far
is ($ars->count, 6, 'Simple count works');

# test LIMIT support
my $lim = $ars->search( {},
  {
    rows => 3,
    offset => 4,
    order_by => 'artistid'
  }
);
is( $lim->count, 2, 'ROWS+OFFSET count ok' );
is( $lim->all, 2, 'Number of ->all objects matches count' );

# test iterator
$lim->reset;
is( $lim->next->artistid, 101, "iterator->next ok" );
is( $lim->next->artistid, 102, "iterator->next ok" );
is( $lim->next, undef, "next past end of resultset ok" );


done_testing;

# clean up our mess
END {
    my $dbh = eval { $schema->storage->_dbh };
    $dbh->do("DROP TABLE artist") if $dbh;
}
