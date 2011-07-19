# Test to ensure we get a consistent result set wether or not we use the
# prefetch option in combination rows (LIMIT).
use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();


my $no_prefetch = $schema->resultset('Artist')->search(
  [   # search deliberately contrived
    { 'artwork.cd_id' => undef },
    { 'tracks.title' => { '!=' => 'blah-blah-1234568' }}
  ],
  { rows => 3, join => { cds => [qw/artwork tracks/] },
 }
);

my $use_prefetch = $no_prefetch->search(
  {},
  {
    select => ['me.artistid', 'me.name'],
    as => ['artistid', 'name'],
    prefetch => 'cds',
    order_by => { -desc => 'name' },
  }
);

is($no_prefetch->count, $use_prefetch->count, '$no_prefetch->count == $use_prefetch->count');
is(
  scalar ($no_prefetch->all),
  scalar ($use_prefetch->all),
  "Amount of returned rows is right"
);

my $artist_many_cds = $schema->resultset('Artist')->search ( {}, {
  join => 'cds',
  group_by => 'me.artistid',
  having => \ 'count(cds.cdid) > 1',
})->first;


$no_prefetch = $schema->resultset('Artist')->search(
  { artistid => $artist_many_cds->id },
  { rows => 1 }
);

$use_prefetch = $no_prefetch->search ({}, { prefetch => 'cds' });

my $normal_artist = $no_prefetch->single;
my $prefetch_artist = $use_prefetch->find({ name => $artist_many_cds->name });
my $prefetch2_artist = $use_prefetch->first;

is(
  $prefetch_artist->cds->count,
  $normal_artist->cds->count,
  "Count of child rel with prefetch + rows => 1 is right (find)"
);
is(
  $prefetch2_artist->cds->count,
  $normal_artist->cds->count,
  "Count of child rel with prefetch + rows => 1 is right (first)"
);

is (
  scalar ($prefetch_artist->cds->all),
  scalar ($normal_artist->cds->all),
  "Amount of child rel rows with prefetch + rows => 1 is right (find)"
);
is (
  scalar ($prefetch2_artist->cds->all),
  scalar ($normal_artist->cds->all),
  "Amount of child rel rows with prefetch + rows => 1 is right (first)"
);

throws_ok (
  sub { $use_prefetch->single },
  qr/resultsets prefetching has_many/,
  'single() with multiprefetch is illegal',
);

my $artist = $use_prefetch->search({'cds.title' => $artist_many_cds->cds->first->title })->next;
is($artist->cds->count, 1, "count on search limiting prefetched has_many");

# try with double limit
my $artist2 = $use_prefetch->search({'cds.title' => { '!=' => $artist_many_cds->cds->first->title } })->slice (0,0)->next;
is($artist2->cds->count, 2, "count on search limiting prefetched has_many");

done_testing;
