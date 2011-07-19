use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

# tests stolen from 748informix.t

my ($dsn, $user, $pass)    = @ENV{map { "DBICTEST_SYBASE_ASA_${_}" }      qw/DSN USER PASS/};
my ($dsn2, $user2, $pass2) = @ENV{map { "DBICTEST_SYBASE_ASA_ODBC_${_}" } qw/DSN USER PASS/};

plan skip_all => <<'EOF' unless $dsn || $dsn2;
Set $ENV{DBICTEST_SYBASE_ASA_DSN} and/or $ENV{DBICTEST_SYBASE_ASA_ODBC_DSN},
_USER and _PASS to run these tests
EOF

my @info = (
  [ $dsn,  $user,  $pass  ],
  [ $dsn2, $user2, $pass2 ],
);

my @handles_to_clean;

foreach my $info (@info) {
  my ($dsn, $user, $pass) = @$info;

  next unless $dsn;

  my $schema = DBICTest::Schema->connect($dsn, $user, $pass, {
    auto_savepoint => 1
  });

  my $dbh = $schema->storage->dbh;

  push @handles_to_clean, $dbh;

  eval { $dbh->do("DROP TABLE artist") };

  $dbh->do(<<EOF);
  CREATE TABLE artist (
    artistid INT IDENTITY PRIMARY KEY,
    name VARCHAR(255) NULL,
    charfield CHAR(10) NULL,
    rank INT DEFAULT 13
  )
EOF

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

# test savepoints
  eval {
    $schema->txn_do(sub {
      eval {
        $schema->txn_do(sub {
          $ars->create({ name => 'in_savepoint' });
          die "rolling back savepoint";
        });
      };
      ok ((not $ars->search({ name => 'in_savepoint' })->first),
        'savepoint rolled back');
      $ars->create({ name => 'in_outer_txn' });
      die "rolling back outer txn";
    });
  };

  like $@, qr/rolling back outer txn/,
    'correct exception for rollback';

  ok ((not $ars->search({ name => 'in_outer_txn' })->first),
    'outer txn rolled back');

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

# test empty insert
  {
    local $ars->result_source->column_info('artistid')->{is_auto_increment} = 0;

    lives_ok { $ars->create({}) }
      'empty insert works';
  }

# test blobs (stolen from 73oracle.t)
  eval { $dbh->do('DROP TABLE bindtype_test') };
  $dbh->do(qq[
  CREATE TABLE bindtype_test
  (
    id    INT          NOT NULL PRIMARY KEY,
    bytea INT          NULL,
    blob  LONG BINARY  NULL,
    clob  LONG VARCHAR NULL
  )
  ],{ RaiseError => 1, PrintError => 1 });

  my %binstr = ( 'small' => join('', map { chr($_) } ( 1 .. 127 )) );
  $binstr{'large'} = $binstr{'small'} x 1024;

  my $maxloblen = length $binstr{'large'};
  local $dbh->{'LongReadLen'} = $maxloblen;

  my $rs = $schema->resultset('BindType');
  my $id = 0;

  foreach my $type (qw( blob clob )) {
    foreach my $size (qw( small large )) {
      $id++;

# turn off horrendous binary DBIC_TRACE output
      local $schema->storage->{debug} = 0;

      lives_ok { $rs->create( { 'id' => $id, $type => $binstr{$size} } ) }
      "inserted $size $type without dying";

      ok($rs->find($id)->$type eq $binstr{$size}, "verified inserted $size $type" );
    }
  }
}

done_testing;

# clean up our mess
END {
  foreach my $dbh (@handles_to_clean) {
    eval { $dbh->do("DROP TABLE $_") } for qw/artist bindtype_test/;
  }
}
