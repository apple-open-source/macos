#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests exercise the handling of collections in the exporter option lists.

=cut

use Test::More tests => 3;
use Data::OptList qw(mkopt_hash);

BEGIN { use_ok('Sub::Exporter'); }

sub is_defined {
  my ($class, $value, $arg) = @_;
  return defined $value;
}

my $counter = 0;

my $config = {
  exports    => [ qw(circsaw drill handsaw nailgun) ],
  collectors => [
    INIT => sub {
      my ($value, $arg) = @_;
      return 0 if @{$arg->{import_args}}; # in other words, fail if args
      $_[0] = [ $counter++ ];
      return 1;
    },
  ]
};

$config->{$_} = mkopt_hash($config->{$_}) for qw(exports collectors);

{
  my $collection = Sub::Exporter::_collect_collections(
    $config, 
    [ ],
    'main',
  );

  is_deeply(
    $collection,
    { INIT => [ 0 ] },
    "collection returned properly from collector",
  );
}

{
  my $collection = eval {
    Sub::Exporter::_collect_collections(
      $config, 
      [ [ handsaw => undef ] ],
      'main',
    );
  };

  like(
    $@,
    qr/INIT failed/,
    "the init collector is run even when other things are here",
  );
}
