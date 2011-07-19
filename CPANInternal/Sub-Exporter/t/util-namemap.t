#!perl -T
use strict;
use warnings;

use Test::More skip_all => 'not actually offerring this feature yet';

# use Test::More tests => 3;

BEGIN { use_ok("Sub::Exporter::Util", 'name_map'); }

is_deeply(
  {
    name_map(
      '_?_gen'  => [ qw(fee fie) ],
      '_make_?' => [ qw(foo bar) ],
    ),
  },
  {
    fee => \'_fee_gen',
    fie => \'_fie_gen',
    foo => \'_make_foo',
    bar => \'_make_bar',
  },
  'example from docs works just dandy',
);

eval { name_map(foo => [ qw(bar) ] ) };
like($@, qr/no \?/, 'exception raised with no ? in template');
