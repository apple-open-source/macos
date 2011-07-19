use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_MSSQL_ADO_${_}" } qw/DSN USER PASS/};

plan skip_all => 'Set $ENV{DBICTEST_MSSQL_ADO_DSN}, _USER and _PASS to run this test'
  unless ($dsn && $user);

my $schema = DBICTest::Schema->connect($dsn, $user, $pass);
$schema->storage->ensure_connected;

isa_ok( $schema->storage, 'DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server' );

$schema->storage->dbh_do (sub {
    my ($storage, $dbh) = @_;
    eval { $dbh->do("DROP TABLE artist") };
    $dbh->do(<<'SQL');
CREATE TABLE artist (
   artistid INT IDENTITY NOT NULL,
   name VARCHAR(100),
   rank INT NOT NULL DEFAULT '13',
   charfield CHAR(10) NULL,
   primary key(artistid)
)
SQL
});

my $new = $schema->resultset('Artist')->create({ name => 'foo' });
ok($new->artistid > 0, 'Auto-PK worked');

# make sure select works
my $found = $schema->resultset('Artist')->search({ name => 'foo' })->first;
is $found->artistid, $new->artistid, 'search works';

# test large column list in select
$found = $schema->resultset('Artist')->search({ name => 'foo' }, {
  select => ['artistid', 'name', map "'foo' foo_$_", 0..50],
  as     => ['artistid', 'name', map       "foo_$_", 0..50],
})->first;
is $found->artistid, $new->artistid, 'select with big column list';
is $found->get_column('foo_50'), 'foo', 'last item in big column list';

# create a few more rows
for (1..12) {
  $schema->resultset('Artist')->create({ name => 'Artist ' . $_ });
}

# test multiple active cursors
my $rs1 = $schema->resultset('Artist')->search({}, { order_by => 'artistid' });
my $rs2 = $schema->resultset('Artist')->search({}, { order_by => 'name' });

while ($rs1->next) {
  ok eval { $rs2->next }, 'multiple active cursors';
}

# test bug where ADO blows up if the first bindparam is shorter than the second
is $schema->resultset('Artist')->search({ artistid => 2 })->first->name,
  'Artist 1',
  'short bindparam';

is $schema->resultset('Artist')->search({ artistid => 13 })->first->name,
  'Artist 12',
  'longer bindparam';

done_testing;

# clean up our mess
END {
  if (my $dbh = eval { $schema->storage->_dbh }) {
    eval { $dbh->do("DROP TABLE $_") }
      for qw/artist/;
  }
}
# vim:sw=2 sts=2
