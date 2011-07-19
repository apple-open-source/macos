use strict;
use warnings;  

# use this if you keep a copy of DBD::Sybase linked to FreeTDS somewhere else
BEGIN {
  if (my $lib_dirs = $ENV{DBICTEST_MSSQL_PERL5LIB}) {
    unshift @INC, $_ for split /:/, $lib_dirs;
  }
}

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_MSSQL_${_}" } qw/DSN USER PASS/};

plan skip_all => 'Set $ENV{DBICTEST_MSSQL_DSN}, _USER and _PASS to run this test'
  unless ($dsn);

my @storage_types = (
  'DBI::Sybase::Microsoft_SQL_Server',
  'DBI::Sybase::Microsoft_SQL_Server::NoBindVars',
);
my $storage_idx = -1;
my $schema;

my $NUMBER_OF_TESTS_IN_BLOCK = 18;
for my $storage_type (@storage_types) {
  $storage_idx++;

  $schema = DBICTest::Schema->clone;

  $schema->connection($dsn, $user, $pass);

  if ($storage_idx != 0) { # autodetect
    no warnings 'redefine';
    local *DBIx::Class::Storage::DBI::_typeless_placeholders_supported =
      sub { 0 };
#    $schema->storage_type("::$storage_type");
    $schema->storage->ensure_connected;
  }
  else {
    $schema->storage->ensure_connected;
  }

  if ($storage_idx == 0 && ref($schema->storage) =~ /NoBindVars\z/) {
    my $tb = Test::More->builder;
    $tb->skip('no placeholders') for 1..$NUMBER_OF_TESTS_IN_BLOCK;
    next;
  }

  isa_ok($schema->storage, "DBIx::Class::Storage::$storage_type");

# start disconnected to test reconnection
  $schema->storage->_dbh->disconnect;

  my $dbh;
  lives_ok (sub {
    $dbh = $schema->storage->dbh;
  }, 'reconnect works');

  $dbh->do("IF OBJECT_ID('artist', 'U') IS NOT NULL
      DROP TABLE artist");
  $dbh->do("IF OBJECT_ID('cd', 'U') IS NOT NULL
      DROP TABLE cd");

  $dbh->do("CREATE TABLE artist (artistid INT IDENTITY PRIMARY KEY, name VARCHAR(100), rank INT DEFAULT '13', charfield CHAR(10) NULL);");
  $dbh->do("CREATE TABLE cd (cdid INT IDENTITY PRIMARY KEY, artist INT,  title VARCHAR(100), year VARCHAR(100), genreid INT NULL, single_track INT NULL);");
# Just to test compat shim, Auto is in Core
  $schema->class('Artist')->load_components('PK::Auto::MSSQL');

# Test PK
  my $new = $schema->resultset('Artist')->create( { name => 'foo' } );
  ok($new->artistid, "Auto-PK worked");

# Test LIMIT
  for (1..6) {
      $schema->resultset('Artist')->create( { name => 'Artist ' . $_, rank => $_ } );
  }

  my $it = $schema->resultset('Artist')->search( { },
      { rows     => 3,
        offset   => 2,
        order_by => 'artistid'
      }
  );

# Test ? in data don't get treated as placeholders
  my $cd = $schema->resultset('CD')->create( {
      artist      => 1,
      title       => 'Does this break things?',
      year        => 2007,
  } );
  ok($cd->id, 'Not treating ? in data as placeholders');

  is( $it->count, 3, "LIMIT count ok" );
  ok( $it->next->name, "iterator->next ok" );
  $it->next;
  $it->next;
  is( $it->next, undef, "next past end of resultset ok" );

# test MONEY column support
  $schema->storage->dbh_do (sub {
      my ($storage, $dbh) = @_;
      eval { $dbh->do("DROP TABLE money_test") };
      $dbh->do(<<'SQL');
  CREATE TABLE money_test (
     id INT IDENTITY PRIMARY KEY,
     amount MONEY NULL
  )
SQL

  });

  my $rs = $schema->resultset('Money');

  my $row;
  lives_ok {
    $row = $rs->create({ amount => 100 });
  } 'inserted a money value';

  cmp_ok $rs->find($row->id)->amount, '==', 100, 'money value round-trip';

  lives_ok {
    $row->update({ amount => 200 });
  } 'updated a money value';

  cmp_ok $rs->find($row->id)->amount, '==', 200,
    'updated money value round-trip';

  lives_ok {
    $row->update({ amount => undef });
  } 'updated a money value to NULL';

  is $rs->find($row->id)->amount,
    undef, 'updated money value to NULL round-trip';

  $rs->create({ amount => 300 }) for (1..3);

  # test multiple active statements
  lives_ok {
    my $artist_rs = $schema->resultset('Artist');
    while (my $row = $rs->next) {
      my $artist = $artist_rs->next;
    }
    $rs->reset;
  } 'multiple active statements';

  $rs->delete;

  # test simple transaction with commit
  lives_ok {
    $schema->txn_do(sub {
      $rs->create({ amount => 400 });
    });
  } 'simple transaction';

  cmp_ok $rs->first->amount, '==', 400, 'committed';
  $rs->reset;

  $rs->delete;

  # test rollback
  throws_ok {
    $schema->txn_do(sub {
      $rs->create({ amount => 400 });
      die 'mtfnpy';
    });
  } qr/mtfnpy/, 'simple failed txn';

  is $rs->first, undef, 'rolled back';
  $rs->reset;
}

# test op-induced autoconnect
lives_ok (sub {

  my $schema =  DBICTest::Schema->clone;
  $schema->connection($dsn, $user, $pass);

  my $artist = $schema->resultset ('Artist')->search ({}, { order_by => 'artistid' })->next;
  is ($artist->id, 1, 'Artist retrieved successfully');
}, 'Query-induced autoconnect works');

done_testing;

# clean up our mess
END {
  if (my $dbh = eval { $schema->storage->dbh }) {
    $dbh->do("IF OBJECT_ID('artist', 'U') IS NOT NULL DROP TABLE artist");
    $dbh->do("IF OBJECT_ID('cd', 'U') IS NOT NULL DROP TABLE cd");
    $dbh->do("IF OBJECT_ID('money_test', 'U') IS NOT NULL DROP TABLE money_test");
  }
}
