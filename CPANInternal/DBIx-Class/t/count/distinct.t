use strict;
use warnings;  

use Test::More;
use Test::Exception;

use lib qw(t/lib);

use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();

# The tag Blue is assigned to cds 1 2 3 and 5
# The tag Cheesy is assigned to cds 2 4 and 5
#
# This combination should make some interesting group_by's
#
my $rs;
my $in_rs = $schema->resultset('Tag')->search({ tag => [ 'Blue', 'Cheesy' ] });

for my $get_count (
  sub { shift->count },
  sub { my $crs = shift->count_rs; isa_ok ($crs, 'DBIx::Class::ResultSetColumn'); $crs->next }
) {
  $rs = $schema->resultset('Tag')->search({ tag => 'Blue' });
  is($get_count->($rs), 4, 'Count without DISTINCT');

  $rs = $schema->resultset('Tag')->search({ tag => [ 'Blue', 'Cheesy' ] }, { group_by => 'tag' });
  is($get_count->($rs), 2, 'Count with single column group_by');

  $rs = $schema->resultset('Tag')->search({ tag => [ 'Blue', 'Cheesy' ] }, { group_by => 'cd' });
  is($get_count->($rs), 5, 'Count with another single column group_by');

  $rs = $schema->resultset('Tag')->search({ tag => 'Blue' }, { group_by => [ qw/tag cd/ ]});
  is($get_count->($rs), 4, 'Count with multiple column group_by');

  $rs = $schema->resultset('Tag')->search({ tag => 'Blue' }, { distinct => 1 });
  is($get_count->($rs), 4, 'Count with single column distinct');

  $rs = $schema->resultset('Tag')->search({ tag => { -in => $in_rs->get_column('tag')->as_query } });
  is($get_count->($rs), 7, 'Count with IN subquery');

  $rs = $schema->resultset('Tag')->search({ tag => { -in => $in_rs->get_column('tag')->as_query } }, { group_by => 'tag' });
  is($get_count->($rs), 2, 'Count with IN subquery with outside group_by');

  $rs = $schema->resultset('Tag')->search({ tag => { -in => $in_rs->get_column('tag')->as_query } }, { distinct => 1 });
  is($get_count->($rs), 7, 'Count with IN subquery with outside distinct');

  $rs = $schema->resultset('Tag')->search({ tag => { -in => $in_rs->get_column('tag')->as_query } }, { distinct => 1, select => 'tag' }), 
  is($get_count->($rs), 2, 'Count with IN subquery with outside distinct on a single column');

  $rs = $schema->resultset('Tag')->search({ tag => { -in => $in_rs->search({}, { group_by => 'tag' })->get_column('tag')->as_query } });
  is($get_count->($rs), 7, 'Count with IN subquery with single group_by');

  $rs = $schema->resultset('Tag')->search({ tag => { -in => $in_rs->search({}, { group_by => 'cd' })->get_column('tag')->as_query } });
  is($get_count->($rs), 7, 'Count with IN subquery with another single group_by');

  $rs = $schema->resultset('Tag')->search({ tag => { -in => $in_rs->search({}, { group_by => [ qw/tag cd/ ] })->get_column('tag')->as_query } });
  is($get_count->($rs), 7, 'Count with IN subquery with multiple group_by');

  $rs = $schema->resultset('Tag')->search({ tag => \"= 'Blue'" });
  is($get_count->($rs), 4, 'Count without DISTINCT, using literal SQL');

  $rs = $schema->resultset('Tag')->search({ tag => \" IN ('Blue', 'Cheesy')" }, { group_by => 'tag' });
  is($get_count->($rs), 2, 'Count with literal SQL and single group_by');

  $rs = $schema->resultset('Tag')->search({ tag => \" IN ('Blue', 'Cheesy')" }, { group_by => 'cd' });
  is($get_count->($rs), 5, 'Count with literal SQL and another single group_by');

  $rs = $schema->resultset('Tag')->search({ tag => \" IN ('Blue', 'Cheesy')" }, { group_by => [ qw/tag cd/ ] });
  is($get_count->($rs), 7, 'Count with literal SQL and multiple group_by');

  $rs = $schema->resultset('Tag')->search({ tag => 'Blue' }, { '+select' => { max => 'tagid' }, distinct => 1 });
  is($get_count->($rs), 4, 'Count with +select aggreggate');

  $rs = $schema->resultset('Tag')->search({}, { select => [\'length(me.tag)'], distinct => 1 });
  is($get_count->($rs), 3, 'Count by distinct function result as select literal');
}

throws_ok(
  sub { my $row = $schema->resultset('Tag')->search({}, { select => { distinct => [qw/tag cd/] } })->first },
  qr/select => { distinct => \.\.\. } syntax is not supported for multiple columns/,
  'throw on unsupported syntax'
);

# make sure distinct+func works
{
  my $rs = $schema->resultset('Artist')->search(
    {},
    {
      join => 'cds',
      distinct => 1,
      '+select' => [ { count => 'cds.cdid', -as => 'amount_of_cds' } ],
      '+as' => [qw/num_cds/],
      order_by => { -desc => 'amount_of_cds' },
    }
  );

  is_same_sql_bind (
    $rs->as_query,
    '(
      SELECT me.artistid, me.name, me.rank, me.charfield, COUNT( cds.cdid ) AS amount_of_cds
        FROM artist me LEFT JOIN cd cds ON cds.artist = me.artistid
      GROUP BY me.artistid, me.name, me.rank, me.charfield
      ORDER BY amount_of_cds DESC
    )',
    [],
  );

  is ($rs->next->get_column ('num_cds'), 3, 'Function aliased correctly');
}

# These two rely on the database to throw an exception. This might not be the case one day. Please revise.
dies_ok(sub { my $count = $schema->resultset('Tag')->search({}, { '+select' => \'tagid AS tag_id', distinct => 1 })->count }, 'expecting to die');

done_testing;
