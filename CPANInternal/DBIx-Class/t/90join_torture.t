use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
my $schema = DBICTest->init_schema();

plan tests => 22;

 {
   my $rs = $schema->resultset( 'CD' )->search(
     {
       'producer.name'   => 'blah',
       'producer_2.name' => 'foo',
     },
     {
       'join' => [
         { cd_to_producer => 'producer' },
         { cd_to_producer => 'producer' },
       ],
       'prefetch' => [
         'artist',
         { cd_to_producer => 'producer' },
       ],
     }
   );
  
   eval {
     my @rows = $rs->all();
   };
   is( $@, '' );
 }


my @rs1a_results = $schema->resultset("Artist")->search_related('cds', {title => 'Forkful of bees'}, {order_by => 'title'});
is($rs1a_results[0]->title, 'Forkful of bees', "bare field conditions okay after search related");
my $rs1 = $schema->resultset("Artist")->search({ 'tags.tag' => 'Blue' }, { join => {'cds' => 'tracks'}, prefetch => {'cds' => 'tags'} });
my @artists = $rs1->all;
cmp_ok(@artists, '==', 2, "Two artists returned");

my $rs2 = $rs1->search({ artistid => '1' }, { join => {'cds' => {'cd_to_producer' => 'producer'} } });
my @artists2 = $rs2->search({ 'producer.name' => 'Matt S Trout' });
my @cds = $artists2[0]->cds;
cmp_ok(scalar @cds, '==', 1, "condition based on inherited join okay");

my $rs3 = $rs2->search_related('cds');
cmp_ok(scalar($rs3->all), '==', 45, "All cds for artist returned");


cmp_ok($rs3->count, '==', 45, "All cds for artist returned via count");

my $rs4 = $schema->resultset("CD")->search({ 'artist.artistid' => '1' }, { join => ['tracks', 'artist'], prefetch => 'artist' });
my @rs4_results = $rs4->all;

is($rs4_results[0]->cdid, 1, "correct artist returned");

my $rs5 = $rs4->search({'tracks.title' => 'Sticky Honey'});
is($rs5->count, 1, "search without using previous joins okay");

my $record_rs = $schema->resultset("Artist")->search(undef, { join => 'cds' })->search(undef, { prefetch => { 'cds' => 'tracks' }});
my $record_jp = $record_rs->next;
ok($record_jp, "prefetch on same rel okay");

my $artist = $schema->resultset("Artist")->find(1);
my $cds = $artist->cds;
is($cds->find(2)->title, 'Forkful of bees', "find on has many rs okay");

my $cd = $cds->search({'me.title' => 'Forkful of bees'}, { prefetch => 'tracks' })->first;
my @tracks = $cd->tracks->all;
is(scalar(@tracks), 3, 'right number of prefetched tracks after has many');

#causes ambig col error due to order_by
#my $tracks_rs = $cds->search_related('tracks', { 'tracks.position' => '2', 'disc.title' => 'Forkful of bees' });
#my $first_tracks_rs = $tracks_rs->first;

my $related_rs = $schema->resultset("Artist")->search({ name => 'Caterwauler McCrae' })->search_related('cds', { year => '2001'})->search_related('tracks', { 'position' => '2' });
is($related_rs->first->trackid, '5', 'search related on search related okay');

#causes ambig col error due to order_by
#$related_rs->search({'cd.year' => '2001'}, {join => ['cd', 'cd']})->all;

my $title = $schema->resultset("Artist")->search_related('twokeys')->search_related('cd')->search({'tracks.position' => '2'}, {join => 'tracks', order_by => 'tracks.trackid'})->next->title;
is($title, 'Forkful of bees', 'search relateds with order by okay');

my $prod_rs = $schema->resultset("CD")->find(1)->producers_sorted;
my $prod_rs2 = $prod_rs->search({ name => 'Matt S Trout' });
my $prod_first = $prod_rs2->first;
is($prod_first->id, '1', 'somewhat pointless search on rel with order_by on it okay');

my $prod_map_rs = $schema->resultset("Artist")->find(1)->cds->search_related('cd_to_producer', {}, { join => 'producer', prefetch => 'producer' });
ok($prod_map_rs->next->producer, 'search related with prefetch okay');

my $stupid = $schema->resultset("Artist")->search_related('artist_undirected_maps', {}, { prefetch => 'artist1' })->search_related('mapped_artists')->search_related('cds', {'cds.cdid' => '2'}, { prefetch => 'tracks' });

my $cd_final = $schema->resultset("Artist")->search_related('artist_undirected_maps', {}, { prefetch => 'artist1' })->search_related('mapped_artists')->search_related('cds', {'cds.cdid' => '2'}, { prefetch => 'tracks' })->first;
is($cd_final->cdid, '2', 'bonkers search_related-with-join-midway okay');

# should end up with cds and cds_2 joined
my $merge_rs_1 = $schema->resultset("Artist")->search({ 'cds_2.cdid' => '2' }, { join => ['cds', 'cds'] });
is(scalar(@{$merge_rs_1->{attrs}->{join}}), 2, 'both joins kept');
ok($merge_rs_1->next, 'query on double joined rel runs okay');

# should only end up with cds joined
my $merge_rs_2 = $schema->resultset("Artist")->search({ }, { join => 'cds' })->search({ 'cds.cdid' => '2' }, { join => 'cds' });
is(scalar(@{$merge_rs_2->{attrs}->{join}}), 1, 'only one join kept when inherited');
my $merge_rs_2_cd = $merge_rs_2->next;

eval {

  my @rs_with_prefetch = $schema->resultset('TreeLike')
                                ->search(
    {'me.id' => 1},
    {
    prefetch => [ 'parent', { 'children' => 'parent' } ],
    });

};

ok(!$@, "pathological prefetch ok");

my $rs = $schema->resultset("Artist")->search({}, { join => 'twokeys' });
my $second_search_rs = $rs->search({ 'cds_2.cdid' => '2' }, { join =>
['cds', 'cds'] });
is(scalar(@{$second_search_rs->{attrs}->{join}}), 3, 'both joins kept');
ok($second_search_rs->next, 'query on double joined rel runs okay');

1;
