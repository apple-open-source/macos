use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();
my $sdebug = $schema->storage->debug;

# has_a test
my $cd = $schema->resultset("CD")->find(4);
my ($artist) = ($INC{'DBICTest/HelperRels'}
                  ? $cd->artist
                  : $cd->search_related('artist'));
is($artist->name, 'Random Boy Band', 'has_a search_related ok');

# has_many test with an order_by clause defined
$artist = $schema->resultset("Artist")->find(1);
my @cds = ($INC{'DBICTest/HelperRels'}
             ? $artist->cds
             : $artist->search_related('cds'));
is( $cds[1]->title, 'Spoonful of bees', 'has_many search_related with order_by ok' );

# search_related with additional abstract query
@cds = ($INC{'DBICTest/HelperRels'}
          ? $artist->cds({ title => { like => '%of%' } })
          : $artist->search_related('cds', { title => { like => '%of%' } } )
       );
is( $cds[1]->title, 'Forkful of bees', 'search_related with abstract query ok' );

# creating a related object
if ($INC{'DBICTest/HelperRels.pm'}) {
  $artist->add_to_cds({ title => 'Big Flop', year => 2005 });
} else {
  my $big_flop = $artist->create_related( 'cds', {
      title => 'Big Flop',
      year => 2005,
  } );

 TODO: {
    local $TODO = "Can't fix right now" if $DBIx::Class::VERSION < 0.09;
    lives_ok { $big_flop->genre} "Don't throw exception when col is not loaded after insert";
  };
}

my $big_flop_cd = ($artist->search_related('cds'))[3];
is( $big_flop_cd->title, 'Big Flop', 'create_related ok' );

{ # make sure we are not making pointless select queries when a FK IS NULL
  my $queries = 0;
  $schema->storage->debugcb(sub { $queries++; });
  $schema->storage->debug(1);
  $big_flop_cd->genre; #should not trigger a select query
  is($queries, 0, 'No SELECT made for belongs_to if key IS NULL');
  $big_flop_cd->genre_inefficient; #should trigger a select query
  is($queries, 1, 'SELECT made for belongs_to if key IS NULL when undef_on_null_fk disabled');
  $schema->storage->debug($sdebug);
  $schema->storage->debugcb(undef);
}

my( $rs_from_list ) = $artist->search_related_rs('cds');
isa_ok( $rs_from_list, 'DBIx::Class::ResultSet', 'search_related_rs in list context returns rs' );

( $rs_from_list ) = $artist->cds_rs();
isa_ok( $rs_from_list, 'DBIx::Class::ResultSet', 'relation_rs in list context returns rs' );

# count_related
is( $artist->count_related('cds'), 4, 'count_related ok' );

# set_from_related
my $track = $schema->resultset("Track")->create( {
  trackid => 1,
  cd => 3,
  position => 98,
  title => 'Hidden Track'
} );
$track->set_from_related( cd => $cd );

is($track->disc->cdid, 4, 'set_from_related ok, including alternative accessor' );

$track->set_from_related( cd => undef );

ok( !defined($track->cd), 'set_from_related with undef ok');

TODO: {
    local $TODO = 'accessing $object->rel and set_from_related';
    my $track = $schema->resultset("Track")->new( {} );
    $track->cd;
    $track->set_from_related( cd => $cd ); 
    ok ($track->cd, 'set_from_related ok after using the accessor' );
};

# update_from_related, the same as set_from_related, but it calls update afterwards
$track = $schema->resultset("Track")->create( {
  trackid => 2,
  cd => 3,
  title => 'Hidden Track 2'
} );
$track->update_from_related( cd => $cd );

my $t_cd = ($schema->resultset("Track")->search( cd => 4, title => 'Hidden Track 2' ))[0]->cd;

is( $t_cd->cdid, 4, 'update_from_related ok' );

# find_or_create_related with an existing record
$cd = $artist->find_or_create_related( 'cds', { title => 'Big Flop' } );
is( $cd->year, 2005, 'find_or_create_related on existing record ok' );

# find_or_create_related creating a new record
$cd = $artist->find_or_create_related( 'cds', {
  title => 'Greatest Hits',
  year => 2006,
} );
is( $cd->title, 'Greatest Hits', 'find_or_create_related new record ok' );

@cds = $artist->search_related('cds');
is( ($artist->search_related('cds'))[4]->title, 'Greatest Hits', 'find_or_create_related new record search ok' );

$artist->delete_related( cds => { title => 'Greatest Hits' });
cmp_ok( $schema->resultset("CD")->search( title => 'Greatest Hits' ), '==', 0, 'delete_related ok' );

# find_or_new_related with an existing record
$cd = $artist->find_or_new_related( 'cds', { title => 'Big Flop' } );
is( $cd->year, 2005, 'find_or_new_related on existing record ok' );
ok( $cd->in_storage, 'find_or_new_related on existing record: is in_storage' );

# find_or_new_related instantiating a new record
$cd = $artist->find_or_new_related( 'cds', {
  title => 'Greatest Hits 2: Louder Than Ever',
  year => 2007,
} );
is( $cd->title, 'Greatest Hits 2: Louder Than Ever', 'find_or_new_related new record ok' );
is( $cd->in_storage, 0, 'find_or_new_related on a new record: not in_storage' );

$cd->artist(undef);
my $newartist = $cd->find_or_new_related( 'artist', {
  name => 'Random Boy Band Two',
  artistid => 200,
} );
is($newartist->name, 'Random Boy Band Two', 'find_or_new_related new artist record with id');
is($newartist->id, 200, 'find_or_new_related new artist id set');

lives_ok( 
    sub { 
        my $new_bookmark = $schema->resultset("Bookmark")->new_result( {} );
        my $new_related_link = $new_bookmark->new_related( 'link', {} );
    },
    'No back rel'
);


TODO: {
  local $TODO = "relationship checking needs fixing";
  # try to add a bogus relationship using the wrong cols
  eval {
      DBICTest::Schema::Artist->add_relationship(
          tracks => 'DBICTest::Schema::Track',
          { 'foreign.cd' => 'self.cdid' }
      );
  };
  like($@, qr/Unknown column/, 'failed when creating a rel with invalid key, ok');
}
  
# another bogus relationship using no join condition
eval {
    DBICTest::Schema::Artist->add_relationship( tracks => 'DBICTest::Track' );
};
like($@, qr/join condition/, 'failed when creating a rel without join condition, ok');

# many_to_many helper tests
$cd = $schema->resultset("CD")->find(1);
my @producers = $cd->producers();
is( $producers[0]->name, 'Matt S Trout', 'many_to_many ok' );
is( $cd->producers_sorted->next->name, 'Bob The Builder',
    'sorted many_to_many ok' );
is( $cd->producers_sorted(producerid => 3)->next->name, 'Fred The Phenotype',
    'sorted many_to_many with search condition ok' );

$cd = $schema->resultset('CD')->find(2);
my $prod_rs = $cd->producers();
my $prod_before_count = $schema->resultset('Producer')->count;
is( $prod_rs->count, 0, "CD doesn't yet have any producers" );
my $prod = $schema->resultset('Producer')->find(1);
$cd->add_to_producers($prod);
is( $prod_rs->count(), 1, 'many_to_many add_to_$rel($obj) count ok' );
is( $prod_rs->first->name, 'Matt S Trout',
    'many_to_many add_to_$rel($obj) ok' );
$cd->remove_from_producers($prod);
$cd->add_to_producers($prod, {attribute => 1});
is( $prod_rs->count(), 1, 'many_to_many add_to_$rel($obj, $link_vals) count ok' );
is( $cd->cd_to_producer->first->attribute, 1, 'many_to_many $link_vals ok');
$cd->remove_from_producers($prod);
$cd->set_producers([$prod], {attribute => 2});
is( $prod_rs->count(), 1, 'many_to_many set_$rel($obj, $link_vals) count ok' );
is( $cd->cd_to_producer->first->attribute, 2, 'many_to_many $link_vals ok');
$cd->remove_from_producers($prod);
is( $schema->resultset('Producer')->find(1)->name, 'Matt S Trout',
    "producer object exists after remove of link" );
is( $prod_rs->count, 0, 'many_to_many remove_from_$rel($obj) ok' );
$cd->add_to_producers({ name => 'Testy McProducer' });
is( $schema->resultset('Producer')->count, $prod_before_count+1,
    'add_to_$rel($hash) inserted a new producer' );
is( $prod_rs->count(), 1, 'many_to_many add_to_$rel($hash) count ok' );
is( $prod_rs->first->name, 'Testy McProducer',
    'many_to_many add_to_$rel($hash) ok' );
$cd->add_to_producers({ name => 'Jack Black' });
is( $prod_rs->count(), 2, 'many_to_many add_to_$rel($hash) count ok' );
$cd->set_producers($schema->resultset('Producer')->all);
is( $cd->producers->count(), $prod_before_count+2, 
    'many_to_many set_$rel(@objs) count ok' );
$cd->set_producers($schema->resultset('Producer')->find(1));
is( $cd->producers->count(), 1, 'many_to_many set_$rel($obj) count ok' );
$cd->set_producers([$schema->resultset('Producer')->all]);
is( $cd->producers->count(), $prod_before_count+2, 
    'many_to_many set_$rel(\@objs) count ok' );
$cd->set_producers([$schema->resultset('Producer')->find(1)]);
is( $cd->producers->count(), 1, 'many_to_many set_$rel([$obj]) count ok' );

eval { $cd->remove_from_producers({ fake => 'hash' }); };
like( $@, qr/needs an object/, 'remove_from_$rel($hash) dies correctly' );

eval { $cd->add_to_producers(); };
like( $@, qr/needs an object or hashref/,
      'add_to_$rel(undef) dies correctly' );

# many_to_many stresstest
my $twokey = $schema->resultset('TwoKeys')->find(1,1);
my $fourkey = $schema->resultset('FourKeys')->find(1,2,3,4);

is( $twokey->fourkeys->count, 0, 'twokey has no fourkeys' );
$twokey->add_to_fourkeys($fourkey, { autopilot => 'engaged' });
my $got_fourkey = $twokey->fourkeys({ sensors => 'online' })->first;
is( $twokey->fourkeys->count, 1, 'twokey has one fourkey' );
is( $got_fourkey->$_, $fourkey->$_,
    'fourkeys row has the correct value for column '.$_ )
  for (qw(foo bar hello goodbye sensors));
$twokey->remove_from_fourkeys($fourkey);
is( $twokey->fourkeys->count, 0, 'twokey has no fourkeys' );
is( $twokey->fourkeys_to_twokeys->count, 0,
    'twokey has no links to fourkey' );


my $undef_artist_cd = $schema->resultset("CD")->new_result({ 'title' => 'badgers', 'year' => 2007 });
is($undef_artist_cd->has_column_loaded('artist'), '', 'FK not loaded');
is($undef_artist_cd->search_related('artist')->count, 0, '0=1 search when FK does not exist and object not yet in db');
eval{ 
     $undef_artist_cd->related_resultset('artist')->new({name => 'foo'});
};
is( $@, '', "Object created on a resultset related to not yet inserted object");
lives_ok{
  $schema->resultset('Artwork')->new_result({})->cd;
} 'undef_on_null_fk does not choke on empty conds';

my $def_artist_cd = $schema->resultset("CD")->new_result({ 'title' => 'badgers', 'year' => 2007, artist => undef });
is($def_artist_cd->has_column_loaded('artist'), 1, 'FK loaded');
is($def_artist_cd->search_related('artist')->count, 0, 'closed search on null FK');

# test undirected many-to-many relationship (e.g. "related artists")
my $undir_maps = $schema->resultset("Artist")
                          ->search ({artistid => 1})
                            ->search_related ('artist_undirected_maps');
is($undir_maps->count, 1, 'found 1 undirected map for artist 1');
is_same_sql_bind (
  $undir_maps->as_query,
  '(
    SELECT artist_undirected_maps.id1, artist_undirected_maps.id2
      FROM artist me
      JOIN artist_undirected_map artist_undirected_maps
        ON artist_undirected_maps.id1 = me.artistid OR artist_undirected_maps.id2 = me.artistid
    WHERE ( artistid = ? )
  )',
  [[artistid => 1]],
  'expected join sql produced',
);

$undir_maps = $schema->resultset("Artist")->find(2)->artist_undirected_maps;
is($undir_maps->count, 1, 'found 1 undirected map for artist 2');

my $mapped_rs = $undir_maps->search_related('mapped_artists');

my @art = $mapped_rs->all;

cmp_ok(@art, '==', 2, "Both artist returned from map");

my $searched = $mapped_rs->search({'mapped_artists.artistid' => {'!=', undef}});

cmp_ok($searched->count, '==', 2, "Both artist returned from map after adding another condition");

# check join through cascaded has_many relationships (also empty has_many rels)
$artist = $schema->resultset("Artist")->find(1);
my $trackset = $artist->cds->search_related('tracks');
is($trackset->count, 10, "Correct number of tracks for artist");
is($trackset->all, 10, "Correct number of track objects for artist");

# now see about updating eveything that belongs to artist 2 to artist 3
$artist = $schema->resultset("Artist")->find(2);
my $nartist = $schema->resultset("Artist")->find(3);
cmp_ok($artist->cds->count, '==', 1, "Correct orig #cds for artist");
cmp_ok($nartist->cds->count, '==', 1, "Correct orig #cds for artist");
$artist->cds->update({artist => $nartist->id});
cmp_ok($artist->cds->count, '==', 0, "Correct new #cds for artist");
cmp_ok($nartist->cds->count, '==', 2, "Correct new #cds for artist");

# check if is_foreign_key_constraint attr is set
my $rs_normal = $schema->source('Track');
my $relinfo = $rs_normal->relationship_info ('cd');
cmp_ok($relinfo->{attrs}{is_foreign_key_constraint}, '==', 1, "is_foreign_key_constraint defined for belongs_to relationships.");

my $rs_overridden = $schema->source('ForceForeign');
my $relinfo_with_attr = $rs_overridden->relationship_info ('cd_3');
cmp_ok($relinfo_with_attr->{attrs}{is_foreign_key_constraint}, '==', 0, "is_foreign_key_constraint defined for belongs_to relationships with attr.");

# check that relationships below left join relationships are forced to left joins 
# when traversing multiple belongs_to
my $cds = $schema->resultset("CD")->search({ 'me.cdid' => 5 }, { join => { single_track => 'cd' } });
is($cds->count, 1, "subjoins under left joins force_left (string)");

$cds = $schema->resultset("CD")->search({ 'me.cdid' => 5 }, { join => { single_track => ['cd'] } });
is($cds->count, 1, "subjoins under left joins force_left (arrayref)");

$cds = $schema->resultset("CD")->search({ 'me.cdid' => 5 }, { join => { single_track => { cd => {} } } });
is($cds->count, 1, "subjoins under left joins force_left (hashref)");

done_testing;
