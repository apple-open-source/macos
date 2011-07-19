use strict;
use warnings;  
no warnings 'uninitialized';

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_SYBASE_${_}" } qw/DSN USER PASS/};

my $TESTS = 66 + 2;

if (not ($dsn && $user)) {
  plan skip_all =>
    'Set $ENV{DBICTEST_SYBASE_DSN}, _USER and _PASS to run this test' .
    "\nWarning: This test drops and creates the tables " .
    "'artist', 'money_test' and 'bindtype_test'";
} else {
  plan tests => $TESTS*2 + 1;
}

my @storage_types = (
  'DBI::Sybase::ASE',
  'DBI::Sybase::ASE::NoBindVars',
);
eval "require DBIx::Class::Storage::$_;" for @storage_types;

my $schema;
my $storage_idx = -1;

sub get_schema {
  DBICTest::Schema->connect($dsn, $user, $pass, {
    on_connect_call => [
      [ blob_setup => log_on_update => 1 ], # this is a safer option
    ],
  });
}

my $ping_count = 0;
{
  my $ping = DBIx::Class::Storage::DBI::Sybase::ASE->can('_ping');
  *DBIx::Class::Storage::DBI::Sybase::ASE::_ping = sub {
    $ping_count++;
    goto $ping;
  };
}

for my $storage_type (@storage_types) {
  $storage_idx++;

  unless ($storage_type eq 'DBI::Sybase::ASE') { # autodetect
    DBICTest::Schema->storage_type("::$storage_type");
  }

  $schema = get_schema();

  $schema->storage->ensure_connected;

  if ($storage_idx == 0 &&
      $schema->storage->isa('DBIx::Class::Storage::DBI::Sybase::ASE::NoBindVars')) {
# no placeholders in this version of Sybase or DBD::Sybase (or using FreeTDS)
      my $tb = Test::More->builder;
      $tb->skip('no placeholders') for 1..$TESTS;
      next;
  }

  isa_ok( $schema->storage, "DBIx::Class::Storage::$storage_type" );

  $schema->storage->_dbh->disconnect;
  lives_ok (sub { $schema->storage->dbh }, 'reconnect works');

  $schema->storage->dbh_do (sub {
      my ($storage, $dbh) = @_;
      eval { $dbh->do("DROP TABLE artist") };
      $dbh->do(<<'SQL');
CREATE TABLE artist (
   artistid INT IDENTITY PRIMARY KEY,
   name VARCHAR(100),
   rank INT DEFAULT 13 NOT NULL,
   charfield CHAR(10) NULL
)
SQL
  });

  my %seen_id;

# so we start unconnected
  $schema->storage->disconnect;

# test primary key handling
  my $new = $schema->resultset('Artist')->create({ name => 'foo' });
  ok($new->artistid > 0, "Auto-PK worked");

  $seen_id{$new->artistid}++;

# check redispatch to storage-specific insert when auto-detected storage
  if ($storage_type eq 'DBI::Sybase::ASE') {
    DBICTest::Schema->storage_type('::DBI');
    $schema = get_schema();
  }

  $new = $schema->resultset('Artist')->create({ name => 'Artist 1' });
  is ( $seen_id{$new->artistid}, undef, 'id for Artist 1 is unique' );
  $seen_id{$new->artistid}++;

# inserts happen in a txn, so we make sure it still works inside a txn too
  $schema->txn_begin;

  for (2..6) {
    $new = $schema->resultset('Artist')->create({ name => 'Artist ' . $_ });
    is ( $seen_id{$new->artistid}, undef, "id for Artist $_ is unique" );
    $seen_id{$new->artistid}++;
  }

  $schema->txn_commit;

# test simple count
  is ($schema->resultset('Artist')->count, 7, 'count(*) of whole table ok');

# test LIMIT support
  my $it = $schema->resultset('Artist')->search({
    artistid => { '>' => 0 }
  }, {
    rows => 3,
    order_by => 'artistid',
  });

  is( $it->count, 3, "LIMIT count ok" );

  is( $it->next->name, "foo", "iterator->next ok" );
  $it->next;
  is( $it->next->name, "Artist 2", "iterator->next ok" );
  is( $it->next, undef, "next past end of resultset ok" );

# now try with offset
  $it = $schema->resultset('Artist')->search({}, {
    rows => 3,
    offset => 3,
    order_by => 'artistid',
  });

  is( $it->count, 3, "LIMIT with offset count ok" );

  is( $it->next->name, "Artist 3", "iterator->next ok" );
  $it->next;
  is( $it->next->name, "Artist 5", "iterator->next ok" );
  is( $it->next, undef, "next past end of resultset ok" );

# now try a grouped count
  $schema->resultset('Artist')->create({ name => 'Artist 6' })
    for (1..6);

  $it = $schema->resultset('Artist')->search({}, {
    group_by => 'name'
  });

  is( $it->count, 7, 'COUNT of GROUP_BY ok' );

# do an IDENTITY_INSERT
  {
    no warnings 'redefine';

    my @debug_out;
    local $schema->storage->{debug} = 1;
    local $schema->storage->debugobj->{callback} = sub {
      push @debug_out, $_[1];
    };

    my $txn_used = 0;
    my $txn_commit = \&DBIx::Class::Storage::DBI::txn_commit;
    local *DBIx::Class::Storage::DBI::txn_commit = sub {
      $txn_used = 1;
      goto &$txn_commit;
    };

    $schema->resultset('Artist')
      ->create({ artistid => 999, name => 'mtfnpy' });

    ok((grep /IDENTITY_INSERT/i, @debug_out), 'IDENTITY_INSERT used');

    SKIP: {
      skip 'not testing lack of txn on IDENTITY_INSERT with NoBindVars', 1
        if $storage_type =~ /NoBindVars/i;

      is $txn_used, 0, 'no txn on insert with IDENTITY_INSERT';
    }
  }

# do an IDENTITY_UPDATE
  {
    my @debug_out;
    local $schema->storage->{debug} = 1;
    local $schema->storage->debugobj->{callback} = sub {
      push @debug_out, $_[1];
    };

    lives_and {
      $schema->resultset('Artist')
        ->find(999)->update({ artistid => 555 });
      ok((grep /IDENTITY_UPDATE/i, @debug_out));
    } 'IDENTITY_UPDATE used';
    $ping_count-- if $@;
  }

  my $bulk_rs = $schema->resultset('Artist')->search({
    name => { -like => 'bulk artist %' }
  });

# test insert_bulk using populate.
  SKIP: {
    skip 'insert_bulk not supported', 4
      unless $storage_type !~ /NoBindVars/i;

    lives_ok {
      $schema->resultset('Artist')->populate([
        {
          name => 'bulk artist 1',
          charfield => 'foo',
        },
        {
          name => 'bulk artist 2',
          charfield => 'foo',
        },
        {
          name => 'bulk artist 3',
          charfield => 'foo',
        },
      ]);
    } 'insert_bulk via populate';

    is $bulk_rs->count, 3, 'correct number inserted via insert_bulk';

    is ((grep $_->charfield eq 'foo', $bulk_rs->all), 3,
      'column set correctly via insert_bulk');

    my %bulk_ids;
    @bulk_ids{map $_->artistid, $bulk_rs->all} = ();

    is ((scalar keys %bulk_ids), 3,
      'identities generated correctly in insert_bulk');

    $bulk_rs->delete;
  }

# make sure insert_bulk works a second time on the same connection
  SKIP: {
    skip 'insert_bulk not supported', 3
      unless $storage_type !~ /NoBindVars/i;

    lives_ok {
      $schema->resultset('Artist')->populate([
        {
          name => 'bulk artist 1',
          charfield => 'bar',
        },
        {
          name => 'bulk artist 2',
          charfield => 'bar',
        },
        {
          name => 'bulk artist 3',
          charfield => 'bar',
        },
      ]);
    } 'insert_bulk via populate called a second time';

    is $bulk_rs->count, 3,
      'correct number inserted via insert_bulk';

    is ((grep $_->charfield eq 'bar', $bulk_rs->all), 3,
      'column set correctly via insert_bulk');

    $bulk_rs->delete;
  }

# test invalid insert_bulk (missing required column)
#
# There should be a rollback, reconnect and the next valid insert_bulk should
# succeed.
  throws_ok {
    $schema->resultset('Artist')->populate([
      {
        charfield => 'foo',
      }
    ]);
  } qr/no value or default|does not allow null|placeholders/i,
# The second pattern is the error from fallback to regular array insert on
# incompatible charset.
# The third is for ::NoBindVars with no syb_has_blk.
  'insert_bulk with missing required column throws error';

# now test insert_bulk with IDENTITY_INSERT
  SKIP: {
    skip 'insert_bulk not supported', 3
      unless $storage_type !~ /NoBindVars/i;

    lives_ok {
      $schema->resultset('Artist')->populate([
        {
          artistid => 2001,
          name => 'bulk artist 1',
          charfield => 'foo',
        },
        {
          artistid => 2002,
          name => 'bulk artist 2',
          charfield => 'foo',
        },
        {
          artistid => 2003,
          name => 'bulk artist 3',
          charfield => 'foo',
        },
      ]);
    } 'insert_bulk with IDENTITY_INSERT via populate';

    is $bulk_rs->count, 3,
      'correct number inserted via insert_bulk with IDENTITY_INSERT';

    is ((grep $_->charfield eq 'foo', $bulk_rs->all), 3,
      'column set correctly via insert_bulk with IDENTITY_INSERT');

    $bulk_rs->delete;
  }

# test correlated subquery
  my $subq = $schema->resultset('Artist')->search({ artistid => { '>' => 3 } })
    ->get_column('artistid')
    ->as_query;
  my $subq_rs = $schema->resultset('Artist')->search({
    artistid => { -in => $subq }
  });
  is $subq_rs->count, 11, 'correlated subquery';

# mostly stolen from the blob stuff Nniuq wrote for t/73oracle.t
  SKIP: {
    skip 'TEXT/IMAGE support does not work with FreeTDS', 22
      if $schema->storage->using_freetds;

    my $dbh = $schema->storage->_dbh;
    {
      local $SIG{__WARN__} = sub {};
      eval { $dbh->do('DROP TABLE bindtype_test') };

      $dbh->do(qq[
        CREATE TABLE bindtype_test 
        (
          id    INT   IDENTITY PRIMARY KEY,
          bytea IMAGE NULL,
          blob  IMAGE NULL,
          clob  TEXT  NULL
        )
      ],{ RaiseError => 1, PrintError => 0 });
    }

    my %binstr = ( 'small' => join('', map { chr($_) } ( 1 .. 127 )) );
    $binstr{'large'} = $binstr{'small'} x 1024;

    my $maxloblen = length $binstr{'large'};
    
    if (not $schema->storage->using_freetds) {
      $dbh->{'LongReadLen'} = $maxloblen * 2;
    } else {
      $dbh->do("set textsize ".($maxloblen * 2));
    }

    my $rs = $schema->resultset('BindType');
    my $last_id;

    foreach my $type (qw(blob clob)) {
      foreach my $size (qw(small large)) {
        no warnings 'uninitialized';

        my $created;
        lives_ok {
          $created = $rs->create( { $type => $binstr{$size} } )
        } "inserted $size $type without dying";

        $last_id = $created->id if $created;

        lives_and {
          ok($rs->find($last_id)->$type eq $binstr{$size})
        } "verified inserted $size $type";
      }
    }

    $rs->delete;

    # blob insert with explicit PK
    # also a good opportunity to test IDENTITY_INSERT
    lives_ok {
      $rs->create( { id => 1, blob => $binstr{large} } )
    } 'inserted large blob without dying with manual PK';

    lives_and {
      ok($rs->find(1)->blob eq $binstr{large})
    } 'verified inserted large blob with manual PK';

    # try a blob update
    my $new_str = $binstr{large} . 'mtfnpy';

    # check redispatch to storage-specific update when auto-detected storage
    if ($storage_type eq 'DBI::Sybase::ASE') {
      DBICTest::Schema->storage_type('::DBI');
      $schema = get_schema();
    }

    lives_ok {
      $rs->search({ id => 1 })->update({ blob => $new_str })
    } 'updated blob successfully';

    lives_and {
      ok($rs->find(1)->blob eq $new_str)
    } 'verified updated blob';

    # try a blob update with IDENTITY_UPDATE
    lives_and {
      $new_str = $binstr{large} . 'hlagh';
      $rs->find(1)->update({ id => 999, blob => $new_str });
      ok($rs->find(999)->blob eq $new_str);
    } 'verified updated blob with IDENTITY_UPDATE';

    ## try multi-row blob update
    # first insert some blobs
    $new_str = $binstr{large} . 'foo';
    lives_and {
      $rs->delete;
      $rs->create({ blob => $binstr{large} }) for (1..2);
      $rs->update({ blob => $new_str });
      is((grep $_->blob eq $new_str, $rs->all), 2);
    } 'multi-row blob update';

    $rs->delete;

    # now try insert_bulk with blobs and only blobs
    $new_str = $binstr{large} . 'bar';
    lives_ok {
      $rs->populate([
        {
          bytea => 1,
          blob => $binstr{large},
          clob => $new_str,
        },
        {
          bytea => 1,
          blob => $binstr{large},
          clob => $new_str,
        },
      ]);
    } 'insert_bulk with blobs does not die';

    is((grep $_->blob eq $binstr{large}, $rs->all), 2,
      'IMAGE column set correctly via insert_bulk');

    is((grep $_->clob eq $new_str, $rs->all), 2,
      'TEXT column set correctly via insert_bulk');

    # now try insert_bulk with blobs and a non-blob which also happens to be an
    # identity column
    SKIP: {
      skip 'no insert_bulk without placeholders', 4
        if $storage_type =~ /NoBindVars/i;

      $rs->delete;
      $new_str = $binstr{large} . 'bar';
      lives_ok {
        $rs->populate([
          {
            id => 1,
            bytea => 1,
            blob => $binstr{large},
            clob => $new_str,
          },
          {
            id => 2,
            bytea => 1,
            blob => $binstr{large},
            clob => $new_str,
          },
        ]);
      } 'insert_bulk with blobs and explicit identity does NOT die';

      is((grep $_->blob eq $binstr{large}, $rs->all), 2,
        'IMAGE column set correctly via insert_bulk with identity');

      is((grep $_->clob eq $new_str, $rs->all), 2,
        'TEXT column set correctly via insert_bulk with identity');

      is_deeply [ map $_->id, $rs->all ], [ 1,2 ],
        'explicit identities set correctly via insert_bulk with blobs';
    }

    lives_and {
      $rs->delete;
      $rs->create({ blob => $binstr{large} }) for (1..2);
      $rs->update({ blob => undef });
      is((grep !defined($_->blob), $rs->all), 2);
    } 'blob update to NULL';
  }

# test MONEY column support (and some other misc. stuff)
  $schema->storage->dbh_do (sub {
      my ($storage, $dbh) = @_;
      eval { $dbh->do("DROP TABLE money_test") };
      $dbh->do(<<'SQL');
CREATE TABLE money_test (
   id INT IDENTITY PRIMARY KEY,
   amount MONEY DEFAULT $999.99 NULL
)
SQL
  });

  my $rs = $schema->resultset('Money');

# test insert with defaults
  lives_and {
    $rs->create({});
    is((grep $_->amount == 999.99, $rs->all), 1);
  } 'insert with all defaults works';
  $rs->delete;

# test insert transaction when there's an active cursor
  {
    my $artist_rs = $schema->resultset('Artist');
    $artist_rs->first;
    lives_ok {
      my $row = $schema->resultset('Money')->create({ amount => 100 });
      $row->delete;
    } 'inserted a row with an active cursor';
    $ping_count-- if $@; # dbh_do calls ->connected
  }

# test insert in an outer transaction when there's an active cursor
  TODO: {
    local $TODO = 'this should work once we have eager cursors';

# clear state, or we get a deadlock on $row->delete
# XXX figure out why this happens
    $schema->storage->disconnect;

    lives_ok {
      $schema->txn_do(sub {
        my $artist_rs = $schema->resultset('Artist');
        $artist_rs->first;
        my $row = $schema->resultset('Money')->create({ amount => 100 });
        $row->delete;
      });
    } 'inserted a row with an active cursor in outer txn';
    $ping_count-- if $@; # dbh_do calls ->connected
  }

# Now test money values.
  my $row;
  lives_ok {
    $row = $rs->create({ amount => 100 });
  } 'inserted a money value';

  is eval { $rs->find($row->id)->amount }, 100, 'money value round-trip';

  lives_ok {
    $row->update({ amount => 200 });
  } 'updated a money value';

  is eval { $rs->find($row->id)->amount },
    200, 'updated money value round-trip';

  lives_ok {
    $row->update({ amount => undef });
  } 'updated a money value to NULL';

  my $null_amount = eval { $rs->find($row->id)->amount };
  ok(
    (($null_amount == undef) && (not $@)),
    'updated money value to NULL round-trip'
  );
  diag $@ if $@;

# Test computed columns and timestamps
  $schema->storage->dbh_do (sub {
      my ($storage, $dbh) = @_;
      eval { $dbh->do("DROP TABLE computed_column_test") };
      $dbh->do(<<'SQL');
CREATE TABLE computed_column_test (
   id INT IDENTITY PRIMARY KEY,
   a_computed_column AS getdate(),
   a_timestamp timestamp,
   charfield VARCHAR(20) DEFAULT 'foo' 
)
SQL
  });

  require DBICTest::Schema::ComputedColumn;
  $schema->register_class(
    ComputedColumn => 'DBICTest::Schema::ComputedColumn'
  );

  ok (($rs = $schema->resultset('ComputedColumn')),
    'got rs for ComputedColumn');

  lives_ok { $row = $rs->create({}) }
    'empty insert for a table with computed columns survived';

  lives_ok {
    $row->update({ charfield => 'bar' })
  } 'update of a table with computed columns survived';
}

is $ping_count, 0, 'no pings';

# clean up our mess
END {
  if (my $dbh = eval { $schema->storage->_dbh }) {
    eval { $dbh->do("DROP TABLE $_") }
      for qw/artist bindtype_test money_test computed_column_test/;
  }
}
