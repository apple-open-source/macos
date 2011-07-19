#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests check export group expansion, specifically the expansion of groups
that use group generators, more specifically when actually imported.

=cut

use Test::More tests => 8;

use lib 't/lib';

use Carp;

BEGIN {
  local $SIG{__DIE__} = sub { Carp::confess @_ };
  use_ok('Test::SubExporter::GroupGen');
  Test::SubExporter::GroupGen->import(
    col1 => { value => 2 },
    -generated => { xyz => 1 },
    -generated => { xyz => 5, -prefix => 'five_' },
    -symbolic  => { xyz => 2 },
  );

  use_ok('Test::SubExporter::GroupGenSubclass');
  Test::SubExporter::GroupGenSubclass->import(
    col1 => { value => 3 },
    -symbolic  => { -prefix => 'subclass_', xyz => 4 },
  );
}

for my $routine (qw(foo bar)) {
  is_deeply(
    main->$routine(),
    {
      name  => $routine,
      class => 'Test::SubExporter::GroupGen',
      group => 'generated',
      arg   => { xyz => 1 }, 
      collection => { col1 => { value => 2 } },
    },
    "generated $routine does what we expect",
  );

  my $five = "five_$routine";
  is_deeply(
    main->$five(),
    {
      name  => $routine,
      class => 'Test::SubExporter::GroupGen',
      group => 'generated',
      arg   => { xyz => 5 }, 
      collection => { col1 => { value => 2 } },
    },
    "generated $five does what we expect",
  );
}

is_deeply(
  main->baz(),
  {
    name  => 'baz',
    class => 'Test::SubExporter::GroupGen',
    group => 'symbolic',
    arg   => { xyz => 2 }, 
    collection => { col1 => { value => 2 } },
  },
  "parent class's generated baz does what we expect",
);

is_deeply(
  main->subclass_baz(),
  {
    name  => 'baz-sc',
    class => 'Test::SubExporter::GroupGenSubclass',
    group => 'symbolic',
    arg   => { xyz => 4 }, 
    collection => { col1 => { value => 3 } },
  },
  "inheriting class's generated baz does what we expect",
);
