use strict;
use warnings;
use Test::More;

use lib qw(t/lib);

BEGIN {
  eval { require Test::Memory::Cycle };
  if ($@) {
    plan skip_all => "leak test needs Test::Memory::Cycle";
  } else {
    plan tests => 1;
  }
}

use DBICTest;
use DBICTest::Schema;

import Test::Memory::Cycle;

my $s = DBICTest::Schema->clone;

memory_cycle_ok($s, 'No cycles in schema');
