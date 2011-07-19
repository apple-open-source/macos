use strict;
use warnings;

use Test::More;

use lib qw(t/lib);

use DBICTest;

my $schema = DBICTest->init_schema();
my $cd_rs = $schema->resultset('CD')->search ({}, { rows => 1, order_by => 'cdid' });

my $track_count = $cd_rs->first->tracks->count;

cmp_ok ($track_count, '>', 1, 'First CD has several tracks');

is ($cd_rs->search_related ('tracks')->count, $track_count, 'related->count returns correct number chained off a limited rs');
is (scalar ($cd_rs->search_related ('tracks')->all), $track_count, 'related->all returns correct number of objects chained off a limited rs');


my $joined_cd_rs = $cd_rs->search ({}, {
  join => 'tracks', rows => 2, distinct => 1, having => \ 'count(tracks.trackid) > 2',
});

my $multiple_track_count = $schema->resultset('Track')->search ({
  cd => { -in => $joined_cd_rs->get_column ('cdid')->as_query }
})->count;


is (
  $joined_cd_rs->search_related ('tracks')->count,
  $multiple_track_count,
  'related->count returns correct number chained off a grouped rs',
);
is (
  scalar ($joined_cd_rs->search_related ('tracks')->all),
  $multiple_track_count,
  'related->all returns correct number of objects chained off a grouped rs',
);

done_testing;
