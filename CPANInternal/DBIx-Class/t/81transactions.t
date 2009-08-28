use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 67;

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
  eval {
    (ref $schema)->txn_do(sub{});
  };
  like($@, qr/storage/, "can't call txn_do without storage");
  eval {
    $schema->txn_do('');
  };
  like($@, qr/must be a CODE reference/, '$coderef parameter check ok');
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

  eval {
    $schema->txn_do($nested_code, $schema, $artist, $code);
  };

  my $error = $@;

  ok(!$error, 'nested txn_do succeeded');
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

  eval {
    $schema->txn_do($fail_code, $artist);
  };

  my $error = $@;

  like($error, qr/the sky is falling/, 'failed txn_do threw an exception');
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

  eval {
    $schema->txn_do($fail_code, $artist);
  };

  my $error = $@;

  like($error, qr/the sky is falling/, 'failed txn_do threw an exception');
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

  eval {
    $schema->txn_do($fail_code, $artist);
  };

  my $error = $@;

  like($error, qr/Rollback failed/, 'failed txn_do with a failed '.
       'txn_rollback threw a rollback exception');
  like($error, qr/the sky is falling/, 'failed txn_do with a failed '.
       'txn_rollback included the original exception');

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

  eval {
    $schema->txn_do($nested_fail_code, $schema, $artist, $code, $fail_code);
  };

  my $error = $@;

  like($error, qr/the sky is falling/, 'nested failed txn_do threw exception');
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
    eval {
        $schema2->txn_begin();
        $schema2->txn_begin();
    };
    my $err = $@;
    ok(($err eq ''), 'Pre-connection nested transactions.');
}

# Test txn_rollback with nested
{
  local $TODO = "Work out how this should work";
  my $local_schema = DBICTest->init_schema();

  my $artist_rs = $local_schema->resultset('Artist');
  throws_ok {
   
    $local_schema->txn_begin;
    $artist_rs->create({ name => 'Test artist rollback 1'});
    $local_schema->txn_begin;
    is($local_schema->storage->transaction_depth, 2, "Correct transaction depth");
    $artist_rs->create({ name => 'Test artist rollback 2'});
    $local_schema->txn_rollback;
  } qr/Not sure what this should be.... something tho/, "Rolled back okay";
  is($local_schema->storage->transaction_depth, 0, "Correct transaction depth");

  ok(!$artist_rs->find({ name => 'Test artist rollback 1'}), "Test Artist not created")
    || $artist_rs->find({ name => 'Test artist rollback 1'})->delete;
}

# Test txn_scope_guard
{
  local $TODO = "Work out how this should work";
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
  } qr/No such column made_up_column.*?line 16/, "Error propogated okay";

  ok(!$artist_rs->find({name => 'Death Cab for Cutie'}), "Artist not created");

  my $inner_exception;
  eval {
    outer($schema, 1);
  };
  is($@, $inner_exception, "Nested exceptions propogated");

  ok(!$artist_rs->find({name => 'Death Cab for Cutie'}), "Artist not created");


  eval {
    # The 0 arg says done die, just let the scope guard go out of scope 
    # forcing a txn_rollback to happen
    outer($schema, 0);
  };
  is($@, "Not sure what we want here, but something", "Rollback okay");

  ok(!$artist_rs->find({name => 'Death Cab for Cutie'}), "Artist not created");

  sub outer {
    my ($schema) = @_;
   
    my $guard = $schema->txn_scope_guard;
    $schema->resultset('Artist')->create({
      name => 'Death Cab for Cutie',
    });
    inner(@_);
    $guard->commit;
  }

  sub inner {
    my ($schema, $fatal) = @_;
    my $guard = $schema->txn_scope_guard;

    my $artist = $artist_rs->find({ name => 'Death Cab for Cutie' });

    is($schema->storage->transaction_depth, 2, "Correct transaction depth");
    undef $@;
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

    # See what happens if we dont $guard->commit;
  }
}
