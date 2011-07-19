use strict;
use warnings;

use lib qw(t/lib);
use Test::More;
use Test::Exception;
use DBICTest;

my $schema = DBICTest->init_schema();


my $cd_rs = $schema->resultset('CD')->search ({ genreid => undef }, { columns => [ 'genreid' ]} );
my $count = $cd_rs->count;
cmp_ok ( $count, '>', 1, 'several CDs with no genre');

my @objects = $cd_rs->all;
is (scalar @objects, $count, 'Correct amount of objects without limit');
isa_ok ($_, 'DBICTest::CD') for @objects;

is_deeply (
  [ map { values %{{$_->get_columns}} } (@objects) ],
  [ (undef) x $count ],
  'All values are indeed undef'
);


isa_ok ($cd_rs->search ({}, { rows => 1 })->single, 'DBICTest::CD');

done_testing;
