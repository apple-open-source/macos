use strict;
use warnings;
use Test::More;

use lib qw(t/lib);

BEGIN {
  require DBIx::Class;
  plan skip_all => 'Test needs: ' . DBIx::Class::Optional::Dependencies->req_missing_for ('test_cycle')
    unless ( DBIx::Class::Optional::Dependencies->req_ok_for ('test_cycle') );
}

use DBICTest;
use DBICTest::Schema;
use Scalar::Util ();

import Test::Memory::Cycle;

my $weak;

{
  my $s = $weak->{schema} = DBICTest->init_schema;
  memory_cycle_ok($s, 'No cycles in schema');

  my $rs = $weak->{resultset} = $s->resultset ('Artist');
  memory_cycle_ok($rs, 'No cycles in resultset');

  my $rsrc = $weak->{resultsource} = $rs->result_source;
  memory_cycle_ok($rsrc, 'No cycles in resultsource');

  my $row = $weak->{row} = $rs->first;
  memory_cycle_ok($row, 'No cycles in row');

  Scalar::Util::weaken ($_) for values %$weak;
  memory_cycle_ok($weak, 'No cycles in weak object collection');
}

for (keys %$weak) {
  ok (! $weak->{$_}, "No $_ leaks");
}

done_testing;
