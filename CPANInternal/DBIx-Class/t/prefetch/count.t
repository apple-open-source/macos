use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

plan tests => 23;

my $schema = DBICTest->init_schema();

my $cd_rs = $schema->resultset('CD')->search (
  { 'tracks.cd' => { '!=', undef } },
  { prefetch => ['tracks', 'artist'] },
);


is($cd_rs->count, 5, 'CDs with tracks count');
is($cd_rs->search_related('tracks')->count, 15, 'Tracks associated with CDs count (before SELECT()ing)');

is($cd_rs->all, 5, 'Amount of CD objects with tracks');
is($cd_rs->search_related('tracks')->count, 15, 'Tracks associated with CDs count (after SELECT()ing)');

is($cd_rs->search_related ('tracks')->all, 15, 'Track objects associated with CDs (after SELECT()ing)');

my $artist = $schema->resultset('Artist')->create({name => 'xxx'});

my $artist_rs = $schema->resultset('Artist')->search(
  {artistid => $artist->id},
  {prefetch=>'cds', join => 'twokeys' }
);

is($artist_rs->count, 1, "New artist found with prefetch turned on");
is(scalar($artist_rs->all), 1, "New artist fetched with prefetch turned on");
is($artist_rs->related_resultset('cds')->count, 0, "No CDs counted on a brand new artist");
is(scalar($artist_rs->related_resultset('cds')->all), 0, "No CDs fetched on a brand new artist (count == fetch)");

# create a cd, and make sure the non-existing join does not skew the count
$artist->create_related ('cds', { title => 'yyy', year => '1999' });
is($artist_rs->related_resultset('cds')->count, 1, "1 CDs counted on a brand new artist");
is(scalar($artist_rs->related_resultset('cds')->all), 1, "1 CDs prefetched on a brand new artist (count == fetch)");

# Really fuck shit up with one more cd and some insanity
# this doesn't quite work as there are the prefetch gets lost
# on search_related. This however is too esoteric to fix right
# now

my $cd2 = $artist->create_related ('cds', {
    title => 'zzz',
    year => '1999',
    tracks => [{ title => 'ping' }, { title => 'pong' }],
});

my $cds = $cd2->search_related ('artist', {}, { join => 'twokeys' })
                  ->search_related ('cds');
my $tracks = $cds->search_related ('tracks');

is($tracks->count, 2, "2 Tracks counted on cd via artist via one of the cds");
is(scalar($tracks->all), 2, "2 Track objects on cd via artist via one of the cds");

is($cds->count, 2, "2 CDs counted on artist via one of the cds");
is(scalar($cds->all), 2, "2 CD objectson artist via one of the cds");

# make sure the join collapses all the way
is_same_sql_bind (
  $tracks->count_rs->as_query,
  '(
    SELECT COUNT( * )
      FROM artist me
      LEFT JOIN twokeys twokeys ON twokeys.artist = me.artistid
      JOIN cd cds ON cds.artist = me.artistid
      JOIN track tracks ON tracks.cd = cds.cdid
    WHERE ( me.artistid = ? )
  )',
  [ [ 'me.artistid' => 4 ] ],
);


TODO: {
  local $TODO = "Chaining with prefetch is fundamentally broken";

  my $queries;
  $schema->storage->debugcb ( sub { $queries++ } );
  $schema->storage->debug (1);

  my $cds = $cd2->search_related ('artist', {}, { prefetch => { cds => 'tracks' }, join => 'twokeys' })
                  ->search_related ('cds');

  my $tracks = $cds->search_related ('tracks');

  is($tracks->count, 2, "2 Tracks counted on cd via artist via one of the cds");
  is(scalar($tracks->all), 2, "2 Tracks prefetched on cd via artist via one of the cds");
  is($tracks->count, 2, "Cached 2 Tracks counted on cd via artist via one of the cds");

  is($cds->count, 2, "2 CDs counted on artist via one of the cds");
  is(scalar($cds->all), 2, "2 CDs prefetched on artist via one of the cds");
  is($cds->count, 2, "Cached 2 CDs counted on artist via one of the cds");

  is ($queries, 3, '2 counts + 1 prefetch?');
}
