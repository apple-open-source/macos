use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

## Real view
my $cds_rs_2000 = $schema->resultset('CD')->search( { year => 2000 });
my $year2kcds_rs = $schema->resultset('Year2000CDs');

is($cds_rs_2000->count, $year2kcds_rs->count, 'View Year2000CDs sees all CDs in year 2000');


## Virtual view
my $cds_rs_1999 = $schema->resultset('CD')->search( { year => 1999 });
my $year1999cds_rs = $schema->resultset('Year1999CDs');

is($cds_rs_1999->count, $year1999cds_rs->count, 'View Year1999CDs sees all CDs in year 1999');


# Test if relationships work correctly
is_deeply (
  [
    $schema->resultset('Year1999CDs')->search (
      {},
      {
        result_class => 'DBIx::Class::ResultClass::HashRefInflator',
        prefetch => ['artist', { tracks => [qw/cd year1999cd year2000cd/] } ],
      },
    )->all
  ],
  [
    $schema->resultset('CD')->search (
      { 'me.year' => '1999'},
      {
        result_class => 'DBIx::Class::ResultClass::HashRefInflator',
        prefetch => ['artist', { tracks => [qw/cd year1999cd year2000cd/] } ],
        columns => [qw/cdid single_track title/],   # to match the columns retrieved by the virtview
      },
    )->all
  ],
  'Prefetch over virtual view gives expected result',
);

is_deeply (
  [
    $schema->resultset('Year2000CDs')->search (
      {},
      {
        result_class => 'DBIx::Class::ResultClass::HashRefInflator',
        prefetch => ['artist', { tracks => [qw/cd year1999cd year2000cd/] } ],
      },
    )->all
  ],
  [
    $schema->resultset('CD')->search (
      { 'me.year' => '2000'},
      {
        result_class => 'DBIx::Class::ResultClass::HashRefInflator',
        prefetch => ['artist', { tracks => [qw/cd year1999cd year2000cd/] } ],
      },
    )->all
  ],
  'Prefetch over regular view gives expected result',
);

done_testing;
