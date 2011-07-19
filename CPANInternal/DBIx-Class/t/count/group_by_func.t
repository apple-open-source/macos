use strict;
use warnings;

use Test::More;

use lib qw(t/lib);

use DBICTest;

my $schema = DBICTest->init_schema();

my $rs = $schema->resultset ('CD')->search ({}, {
  select => [
    { substr => [ 'title', 1, 1 ], -as => 'initial' },
    { count => '*' },
  ],
  as => [qw/title_initial cnt/],
  group_by => ['initial'],
  order_by => { -desc => 'initial' },
  result_class => 'DBIx::Class::ResultClass::HashRefInflator',
});

is_deeply (
  [$rs->all],
  [
    { title_initial => 'S', cnt => '1' },
    { title_initial => 'G', cnt => '1' },
    { title_initial => 'F', cnt => '1' },
    { title_initial => 'C', cnt => '2' },
  ],
  'Correct result',
);

is ($rs->count, 4, 'Correct count');

done_testing;
