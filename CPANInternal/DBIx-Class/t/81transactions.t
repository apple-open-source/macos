use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 39;

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
  my @titles = map {'txn_do test CD ' . $_} (1..5);
  my $artist = $schema->resultset('Artist')->find(1);
  my $count_before = $artist->cds->count;
  my $count_after = $schema->txn_do($code, $artist, @titles);
  is($count_after, $count_before+5, 'successful txn added 5 cds');
  is($artist->cds({
    title => "txn_do test CD $_",
  })->first->year, 2006, "new CD $_ year correct") for (1..5);
}

# Test successful txn_do() - list context
{
  my @titles = map {'txn_do test CD ' . $_} (6..10);
  my $artist = $schema->resultset('Artist')->find(1);
  my $count_before = $artist->cds->count;
  my @cds = $schema->txn_do($code, $artist, @titles);
  is(scalar @cds, $count_before+5, 'added 5 CDs and returned in list context');
  is($artist->cds({
    title => "txn_do test CD $_",
  })->first->year, 2006, "new CD $_ year correct") for (6..10);
}

# Test nested successful txn_do()
{
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
}

# Test failed txn_do() with failed rollback
{
  my $artist = $schema->resultset('Artist')->find(3);

  # Force txn_rollback() to throw an exception
  no warnings 'redefine';
  no strict 'refs';
  local *{"DBIx::Class::Schema::txn_rollback"} = sub{die 'FAILED'};

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
  $schema->storage->{transaction_depth} = 0; # Must reset this or further tests
                                             # will fail
}

# Test nested failed txn_do()
{
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

