use strict;
use warnings;

use Test::More;
use Test::Warn;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $code = sub {
  my ($artist, @cd_titles) = @_;

  $artist->create_related('cds', {
    title => $_,
    year => 2006,
  }) foreach (@cd_titles);

  return $artist->cds->all;
};

# Test checking of parameters
{
  throws_ok (sub {
    (ref $schema)->txn_do(sub{});
  }, qr/storage/, "can't call txn_do without storage");

  throws_ok ( sub {
    $schema->txn_do('');
  }, qr/must be a CODE reference/, '$coderef parameter check ok');
}

# Test successful txn_do() - scalar context
{
  is( $schema->storage->{transaction_depth}, 0, 'txn depth starts at 0');

  my @titles = map {'txn_do test CD ' . $_} (1..5);
  my $artist = $schema->resultset('Artist')->find(1);
  my $count_before = $artist->cds->count;
  my $count_after = $schema->txn_do($code, $artist, @titles);
  is($count_after, $count_before+5, 'successful txn added 5 cds');
  is($artist->cds({
    title => "txn_do test CD $_",
  })->first->year, 2006, "new CD $_ year correct") for (1..5);

  is( $schema->storage->{transaction_depth}, 0, 'txn depth has been reset');
}

# Test successful txn_do() - list context
{
  is( $schema->storage->{transaction_depth}, 0, 'txn depth starts at 0');

  my @titles = map {'txn_do test CD ' . $_} (6..10);
  my $artist = $schema->resultset('Artist')->find(1);
  my $count_before = $artist->cds->count;
  my @cds = $schema->txn_do($code, $artist, @titles);
  is(scalar @cds, $count_before+5, 'added 5 CDs and returned in list context');
  is($artist->cds({
    title => "txn_do test CD $_",
  })->first->year, 2006, "new CD $_ year correct") for (6..10);

  is( $schema->storage->{transaction_depth}, 0, 'txn depth has been reset');
}

# Test nested successful txn_do()
{
  is( $schema->storage->{transaction_depth}, 0, 'txn depth starts at 0');

  my $nested_code = sub {
    my ($schema, $artist, $code) = @_;

    my @titles1 = map {'nested txn_do test CD ' . $_} (1..5);
    my @titles2 = map {'nested txn_do test CD ' . $_} (6..10);

    $schema->txn_do($code, $artist, @titles1);
    $schema->txn_do($code, $artist, @titles2);
  };

  my $artist = $schema->resultset('Artist')->find(2);
  my $count_before = $artist->cds->count;

  lives_ok (sub {
    $schema->txn_do($nested_code, $schema, $artist, $code);
  }, 'nested txn_do succeeded');

  is($artist->cds({
    title => 'nested txn_do test CD '.$_,
  })->first->year, 2006, qq{nested txn_do CD$_ year ok}) for (1..10);
  is($artist->cds->count, $count_before+10, 'nested txn_do added all CDs');

  is( $schema->storage->{transaction_depth}, 0, 'txn depth has been reset');
}

my $fail_code = sub {
  my ($artist) = @_;
  $artist->create_related('cds', {
    title => 'this should not exist',
    year => 2005,
  });
  die "the sky is falling";
};

# Test failed txn_do()
{

  is( $schema->storage->{transaction_depth}, 0, 'txn depth starts at 0');

  my $artist = $schema->resultset('Artist')->find(3);

  throws_ok (sub {
    $schema->txn_do($fail_code, $artist);
  }, qr/the sky is falling/, 'failed txn_do threw an exception');

  my $cd = $artist->cds({
    title => 'this should not exist',
    year => 2005,
  })->first;
  ok(!defined($cd), q{failed txn_do didn't change the cds table});

  is( $schema->storage->{transaction_depth}, 0, 'txn depth has been reset');
}

# do the same transaction again
{
  is( $schema->storage->{transaction_depth}, 0, 'txn depth starts at 0');

  my $artist = $schema->resultset('Artist')->find(3);

  throws_ok (sub {
    $schema->txn_do($fail_code, $artist);
  }, qr/the sky is falling/, 'failed txn_do threw an exception');

  my $cd = $artist->cds({
    title => 'this should not exist',
    year => 2005,
  })->first;
  ok(!defined($cd), q{failed txn_do didn't change the cds table});

  is( $schema->storage->{transaction_depth}, 0, 'txn depth has been reset');
}

# Test failed txn_do() with failed rollback
{
  is( $schema->storage->{transaction_depth}, 0, 'txn depth starts at 0');

  my $artist = $schema->resultset('Artist')->find(3);

  # Force txn_rollback() to throw an exception
  no warnings 'redefine';
  no strict 'refs';

  # die in rollback, but maintain sanity for further tests ...
  local *{"DBIx::Class::Storage::DBI::SQLite::txn_rollback"} = sub{
    my $storage = shift;
    $storage->{transaction_depth}--;
    die 'FAILED';
  };

  throws_ok (
    sub {
      $schema->txn_do($fail_code, $artist);
    },
    qr/the sky is falling.+Rollback failed/s,
    'txn_rollback threw a rollback exception (and included the original exception'
  );

  my $cd = $artist->cds({
    title => 'this should not exist',
    year => 2005,
  })->first;
  isa_ok($cd, 'DBICTest::CD', q{failed txn_do with a failed txn_rollback }.
         q{changed the cds table});
  $cd->delete; # Rollback failed
  $cd = $artist->cds({
    title => 'this should not exist',
    year => 2005,
  })->first;
  ok(!defined($cd), q{deleted the failed txn's cd});
  $schema->storage->_dbh->rollback;
}

# Test nested failed txn_do()
{
  is( $schema->storage->{transaction_depth}, 0, 'txn depth starts at 0');

  my $nested_fail_code = sub {
    my ($schema, $artist, $code1, $code2) = @_;

    my @titles = map {'nested txn_do test CD ' . $_} (1..5);

    $schema->txn_do($code1, $artist, @titles); # successful txn
    $schema->txn_do($code2, $artist);          # failed txn
  };

  my $artist = $schema->resultset('Artist')->find(3);

  throws_ok ( sub {
    $schema->txn_do($nested_fail_code, $schema, $artist, $code, $fail_code);
  }, qr/the sky is falling/, 'nested failed txn_do threw exception');

  ok(!defined($artist->cds({
    title => 'nested txn_do test CD '.$_,
    year => 2006,
  })->first), qq{failed txn_do didn't add first txn's cd $_}) for (1..5);
  my $cd = $artist->cds({
    title => 'this should not exist',
    year => 2005,
  })->first;
  ok(!defined($cd), q{failed txn_do didn't add failed txn's cd});
}

# Grab a new schema to test txn before connect
{
    my $schema2 = DBICTest->init_schema(no_deploy => 1);
    lives_ok (sub {
        $schema2->txn_begin();
        $schema2->txn_begin();
    }, 'Pre-connection nested transactions.');

    # although not connected DBI would still warn about rolling back at disconnect
    $schema2->txn_rollback;
    $schema2->txn_rollback;
    $schema2->storage->disconnect;
}
$schema->storage->disconnect;

# Test txn_scope_guard
{
  my $schema = DBICTest->init_schema();

  is($schema->storage->transaction_depth, 0, "Correct transaction depth");
  my $artist_rs = $schema->resultset('Artist');
  throws_ok {
   my $guard = $schema->txn_scope_guard;


    $artist_rs->create({
      name => 'Death Cab for Cutie',
      made_up_column => 1,
    });

   $guard->commit;
  } qr/No such column made_up_column .*? at .*?81transactions.t line \d+/s, "Error propogated okay";

  ok(!$artist_rs->find({name => 'Death Cab for Cutie'}), "Artist not created");

  my $inner_exception = '';  # set in inner() below
  throws_ok (sub {
    outer($schema, 1);
  }, qr/$inner_exception/, "Nested exceptions propogated");

  ok(!$artist_rs->find({name => 'Death Cab for Cutie'}), "Artist not created");

  lives_ok (sub {
    warnings_exist ( sub {
      # The 0 arg says don't die, just let the scope guard go out of scope
      # forcing a txn_rollback to happen
      outer($schema, 0);
    }, qr/A DBIx::Class::Storage::TxnScopeGuard went out of scope without explicit commit or error. Rolling back./, 'Out of scope warning detected');
    ok(!$artist_rs->find({name => 'Death Cab for Cutie'}), "Artist not created");
  }, 'rollback successful withot exception');

  sub outer {
    my ($schema) = @_;

    my $guard = $schema->txn_scope_guard;
    $schema->resultset('Artist')->create({
      name => 'Death Cab for Cutie',
    });
    inner(@_);
  }

  sub inner {
    my ($schema, $fatal) = @_;

    my $inner_guard = $schema->txn_scope_guard;
    is($schema->storage->transaction_depth, 2, "Correct transaction depth");

    my $artist = $artist_rs->find({ name => 'Death Cab for Cutie' });

    eval {
      $artist->cds->create({
        title => 'Plans',
        year => 2005,
        $fatal ? ( foo => 'bar' ) : ()
      });
    };
    if ($@) {
      # Record what got thrown so we can test it propgates out properly.
      $inner_exception = $@;
      die $@;
    }

    # inner guard should commit without consequences
    $inner_guard->commit;
  }
}

# make sure the guard does not eat exceptions
{
  my $schema = DBICTest->init_schema();
  throws_ok (sub {
    my $guard = $schema->txn_scope_guard;
    $schema->resultset ('Artist')->create ({ name => 'bohhoo'});

    $schema->storage->disconnect;  # this should freak out the guard rollback

    die 'Deliberate exception';
  }, qr/Deliberate exception.+Rollback failed/s);
}

# make sure it warns *big* on failed rollbacks
{
  my $schema = DBICTest->init_schema();

  # something is really confusing Test::Warn here, no time to debug
=begin
  warnings_exist (
    sub {
      my $guard = $schema->txn_scope_guard;
      $schema->resultset ('Artist')->create ({ name => 'bohhoo'});

      $schema->storage->disconnect;  # this should freak out the guard rollback
    },
    [
      qr/A DBIx::Class::Storage::TxnScopeGuard went out of scope without explicit commit or error. Rolling back./,
      qr/\*+ ROLLBACK FAILED\!\!\! \*+/,
    ],
    'proper warnings generated on out-of-scope+rollback failure'
  );
=cut

  my @want = (
    qr/A DBIx::Class::Storage::TxnScopeGuard went out of scope without explicit commit or error. Rolling back./,
    qr/\*+ ROLLBACK FAILED\!\!\! \*+/,
  );

  my @w;
  local $SIG{__WARN__} = sub {
    if (grep {$_[0] =~ $_} (@want)) {
      push @w, $_[0];
    }
    else {
      warn $_[0];
    }
  };
  {
      my $guard = $schema->txn_scope_guard;
      $schema->resultset ('Artist')->create ({ name => 'bohhoo'});

      $schema->storage->disconnect;  # this should freak out the guard rollback
  }

  is (@w, 2, 'Both expected warnings found');
}

# make sure AutoCommit => 0 on external handles behaves correctly with scope_guard
{
  my $factory = DBICTest->init_schema (AutoCommit => 0);
  cmp_ok ($factory->resultset('CD')->count, '>', 0, 'Something to delete');
  my $dbh = $factory->storage->dbh;

  ok (!$dbh->{AutoCommit}, 'AutoCommit is off on $dbh');
  my $schema = DBICTest::Schema->connect (sub { $dbh });


  lives_ok ( sub {
    my $guard = $schema->txn_scope_guard;
    $schema->resultset('CD')->delete;
    $guard->commit;
  }, 'No attempt to start a transaction with scope guard');

  is ($schema->resultset('CD')->count, 0, 'Deletion successful');
}

# make sure AutoCommit => 0 on external handles behaves correctly with txn_do
{
  my $factory = DBICTest->init_schema (AutoCommit => 0);
  cmp_ok ($factory->resultset('CD')->count, '>', 0, 'Something to delete');
  my $dbh = $factory->storage->dbh;

  ok (!$dbh->{AutoCommit}, 'AutoCommit is off on $dbh');
  my $schema = DBICTest::Schema->connect (sub { $dbh });


  lives_ok ( sub {
    $schema->txn_do (sub { $schema->resultset ('CD')->delete });
  }, 'No attempt to start a atransaction with txn_do');

  is ($schema->resultset('CD')->count, 0, 'Deletion successful');
}

done_testing;
