#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests exercise the handling of collections in the exporter option lists.

=cut

use Test::More tests => 8;
use Data::OptList qw(mkopt_hash);

BEGIN { use_ok('Sub::Exporter'); }

sub is_defined {
  my ($class, $value, $arg) = @_;
  return defined $value;
}

my $config = {
  exports => [
    qw(circsaw drill handsaw nailgun),
    hammer => sub { sub { print "BANG BANG BANG\n" } },
  ],
  groups => {
    default => [
      'handsaw',
      'hammer'  => { claw => 1 },
    ],
    cutters => [ qw(circsaw handsaw), circsaw => { as => 'buzzsaw' } ],
  },
  collectors => [
    'defaults',
    brand_preference => sub { 0 },
    model_preference => sub { 1 },
    sets_own_value   => sub { $_[0] = { foo => 10 } },
    definedp         => \'is_defined',
  ]
};

$config->{$_} = mkopt_hash($config->{$_})
  for qw(exports collectors);

{
  my $collection = Sub::Exporter::_collect_collections(
    $config, 
    [ [ circsaw => undef ], [ defaults => { foo => 1, bar => 2 } ] ],
    'main',
  );

  is_deeply(
    $collection,
    { defaults => { foo => 1, bar => 2 } },
    "collection returned properly from collector",
  );
}

{
  my $collection = Sub::Exporter::_collect_collections(
    $config, 
    [ [ sets_own_value => undef ] ],
    'main',
  );

  is_deeply(
    $collection,
    { sets_own_value => { foo => 10} },
    "a collector can alter the stack to change its own value",
  );
}

{
  my $arg = [ [ defaults => [ 1 ] ], [ defaults => { foo => 1, bar => 2 } ] ];

  eval { Sub::Exporter::_collect_collections($config, $arg, 'main'); };
  like(
    $@,
    qr/collection \S+ provided multiple/,
    "can't provide multiple collection values",
  );
}

{
  # because the brand_preference validator always fails, this should die
  my $arg = [ [ brand_preference => [ 1, 2, 3 ] ] ];
  eval { Sub::Exporter::_collect_collections($config, $arg, 'main') };
  like(
    $@,
    qr/brand_preference failed validation/,
    "collector validator prevents bad export"
  );
}

{
  # the definedp collector should require a defined value; this should be ok
  my $arg = [ [ definedp => {} ] ];
  my $collection = Sub::Exporter::_collect_collections($config, $arg, 'main');
  is_deeply(
    $collection,
    { definedp => {} },
    "collector validator allows collection"
  );
}

{
  # the definedp collector should require a defined value; this should die
  my $arg = [ [ definedp => undef ] ];
  eval { Sub::Exporter::_collect_collections($config, $arg, 'main') };
  like(
    $@,
    qr/definedp failed validation/,
    "collector validator prevents bad export"
  );
}

{
  my $arg = [ [ model_preference => [ 1, 2, 3 ] ] ];
  my $collection = Sub::Exporter::_collect_collections($config, $arg, 'main');
  is_deeply(
    $collection,
    { model_preference => [ 1, 2, 3 ] },
    "true-returning validator allows collection",
  );
}
