use strict;
use warnings;

use lib qw(t/lib);
use Test::More;
use Test::Exception;
use DBICTest;

#plan tests => 5;
plan 'no_plan';

my $schema = DBICTest->init_schema();

my $tkfks = $schema->resultset('FourKeys_to_TwoKeys');

my ($fa, $fb) = $tkfks->related_resultset ('fourkeys')->populate ([
  [qw/foo bar hello goodbye sensors read_count/],
  [qw/1   1   1     1       a       10         /],
  [qw/2   2   2     2       b       20         /],
]);

# This is already provided by DBICTest
#my ($ta, $tb) = $tkfk->related_resultset ('twokeys')->populate ([
#  [qw/artist  cd /],
#  [qw/1       1  /],
#  [qw/2       2  /],
#]);
my ($ta, $tb) = $schema->resultset ('TwoKeys')
                  ->search ( [ { artist => 1, cd => 1 }, { artist => 2, cd => 2 } ])
                    ->all;

my $tkfk_cnt = $tkfks->count;

my $non_void_ctx = $tkfks->populate ([
  { autopilot => 'a', fourkeys =>  $fa, twokeys => $ta, pilot_sequence => 10 },
  { autopilot => 'b', fourkeys =>  $fb, twokeys => $tb, pilot_sequence => 20 },
  { autopilot => 'x', fourkeys =>  $fa, twokeys => $tb, pilot_sequence => 30 },
  { autopilot => 'y', fourkeys =>  $fb, twokeys => $ta, pilot_sequence => 40 },
]);
is ($tkfks->count, $tkfk_cnt += 4, 'FourKeys_to_TwoKeys populated succesfully');

#
# Make sure the forced group by works (i.e. the joining does not cause double-updates)
#

# create a resultset matching $fa and $fb only
my $fks = $schema->resultset ('FourKeys')
                  ->search ({ map { $_ => [1, 2] } qw/foo bar hello goodbye/}, { join => 'fourkeys_to_twokeys' });

is ($fks->count, 4, 'Joined FourKey count correct (2x2)');
$fks->update ({ read_count => \ 'read_count + 1' });
$_->discard_changes for ($fa, $fb);

is ($fa->read_count, 11, 'Update ran only once on joined resultset');
is ($fb->read_count, 21, 'Update ran only once on joined resultset');


#
# Make sure multicolumn in or the equivalen functions correctly
#

my $sub_rs = $tkfks->search (
  [
    { map { $_ => 1 } qw/artist.artistid cd.cdid fourkeys.foo fourkeys.bar fourkeys.hello fourkeys.goodbye/ },
    { map { $_ => 2 } qw/artist.artistid cd.cdid fourkeys.foo fourkeys.bar fourkeys.hello fourkeys.goodbye/ },
  ],
  {
    join => [ 'fourkeys', { twokeys => [qw/artist cd/] } ],
  },
);

is ($sub_rs->count, 2, 'Only two rows from fourkeys match');

# attempts to delete a grouped rs should fail miserably
throws_ok (
  sub { $sub_rs->search ({}, { distinct => 1 })->delete },
  qr/attempted a delete operation on a resultset which does group_by/,
  'Grouped rs update/delete not allowed',
);

# grouping on PKs only should pass
$sub_rs->search (
  {},
  {
    group_by => [ reverse $sub_rs->result_source->primary_columns ],     # reverse to make sure the PK-list comaprison works
  },
)->update ({ pilot_sequence => \ 'pilot_sequence + 1' });

is_deeply (
  [ $tkfks->search ({ autopilot => [qw/a b x y/]}, { order_by => 'autopilot' })
            ->get_column ('pilot_sequence')->all 
  ],
  [qw/11 21 30 40/],
  'Only two rows incremented',
);

# also make sure weird scalarref usage works (RT#51409)
$tkfks->search (
  \ 'pilot_sequence BETWEEN 11 AND 21',
)->update ({ pilot_sequence => \ 'pilot_sequence + 1' });

is_deeply (
  [ $tkfks->search ({ autopilot => [qw/a b x y/]}, { order_by => 'autopilot' })
            ->get_column ('pilot_sequence')->all 
  ],
  [qw/12 22 30 40/],
  'Only two rows incremented (where => scalarref works)',
);

$sub_rs->delete;

is ($tkfks->count, $tkfk_cnt -= 2, 'Only two rows deleted');
