use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 78;

eval { require DateTime::Format::MySQL };
my $NO_DTFM = $@ ? 1 : 0;

# figure out if we've got a version of sqlite that is older than 3.2.6, in
# which case COUNT(DISTINCT()) doesn't work
my $is_broken_sqlite = 0;
my ($sqlite_major_ver,$sqlite_minor_ver,$sqlite_patch_ver) =
    split /\./, $schema->storage->dbh->get_info(18);
if( $schema->storage->dbh->get_info(17) eq 'SQLite' &&
    ( ($sqlite_major_ver < 3) ||
      ($sqlite_major_ver == 3 && $sqlite_minor_ver < 2) ||
      ($sqlite_major_ver == 3 && $sqlite_minor_ver == 2 && $sqlite_patch_ver < 6) ) ) {
    $is_broken_sqlite = 1;
}


my @art = $schema->resultset("Artist")->search({ }, { order_by => 'name DESC'});

cmp_ok(@art, '==', 3, "Three artists returned");

my $art = $art[0];

is($art->name, 'We Are Goth', "Correct order too");

$art->name('We Are In Rehab');

is($art->name, 'We Are In Rehab', "Accessor update ok");

is($art->get_column("name"), 'We Are In Rehab', 'And via get_column');

ok($art->update, 'Update run');

my $record_jp = $schema->resultset("Artist")->search(undef, { join => 'cds' })->search(undef, { prefetch => 'cds' })->next;

ok($record_jp, "prefetch on same rel okay");

my $record_fn = $schema->resultset("Artist")->search(undef, { join => 'cds' })->search({'cds.cdid' => '1'}, {join => 'artist_undirected_maps'})->next;

ok($record_fn, "funny join is okay");

@art = $schema->resultset("Artist")->search({ name => 'We Are In Rehab' });

cmp_ok(@art, '==', 1, "Changed artist returned by search");

cmp_ok($art[0]->artistid, '==', 3,'Correct artist too');

$art->delete;

@art = $schema->resultset("Artist")->search({ });

cmp_ok(@art, '==', 2, 'And then there were two');

ok(!$art->in_storage, "It knows it's dead");

eval { $art->delete; };

ok($@, "Can't delete twice: $@");

is($art->name, 'We Are In Rehab', 'But the object is still live');

$art->insert;

ok($art->in_storage, "Re-created");

@art = $schema->resultset("Artist")->search({ });

cmp_ok(@art, '==', 3, 'And now there are three again');

my $new = $schema->resultset("Artist")->create({ artistid => 4 });

cmp_ok($new->artistid, '==', 4, 'Create produced record ok');

@art = $schema->resultset("Artist")->search({ });

cmp_ok(@art, '==', 4, "Oh my god! There's four of them!");

$new->set_column('name' => 'Man With A Fork');

is($new->name, 'Man With A Fork', 'set_column ok');

$new->discard_changes;

ok(!defined $new->name, 'Discard ok');

$new->name('Man With A Spoon');

$new->update;

my $new_again = $schema->resultset("Artist")->find(4);

is($new_again->name, 'Man With A Spoon', 'Retrieved correctly');

is($new_again->ID, 'DBICTest::Artist|artist|artistid=4', 'unique object id generated correctly');

# Test backwards compatibility
{
  my $warnings = '';
  local $SIG{__WARN__} = sub { $warnings .= $_[0] };

  my $artist_by_hash = $schema->resultset('Artist')->find(artistid => 4);
  is($artist_by_hash->name, 'Man With A Spoon', 'Retrieved correctly');
  is($artist_by_hash->ID, 'DBICTest::Artist|artist|artistid=4', 'unique object id generated correctly');
  like($warnings, qr/deprecated/, 'warned about deprecated find usage');
}

is($schema->resultset("Artist")->count, 4, 'count ok');

# test find_or_new
{
  my $existing_obj = $schema->resultset('Artist')->find_or_new({
    artistid => 4,
  });

  is($existing_obj->name, 'Man With A Spoon', 'find_or_new: found existing artist');
  ok($existing_obj->in_storage, 'existing artist is in storage');

  my $new_obj = $schema->resultset('Artist')->find_or_new({
    artistid => 5,
    name     => 'find_or_new',
  });

  is($new_obj->name, 'find_or_new', 'find_or_new: instantiated a new artist');
  ok(! $new_obj->in_storage, 'new artist is not in storage');
}

my $cd = $schema->resultset("CD")->find(1);
my %cols = $cd->get_columns;

cmp_ok(keys %cols, '==', 4, 'get_columns number of columns ok');

is($cols{title}, 'Spoonful of bees', 'get_columns values ok');

%cols = ( title => 'Forkful of bees', year => 2005);
$cd->set_columns(\%cols);

is($cd->title, 'Forkful of bees', 'set_columns ok');

is($cd->year, 2005, 'set_columns ok');

$cd->discard_changes;

# check whether ResultSource->columns returns columns in order originally supplied
my @cd = $schema->source("CD")->columns;

is_deeply( \@cd, [qw/cdid artist title year/], 'column order');

$cd = $schema->resultset("CD")->search({ title => 'Spoonful of bees' }, { columns => ['title'] })->next;
is($cd->title, 'Spoonful of bees', 'subset of columns returned correctly');

$cd = $schema->resultset("CD")->search(undef, { include_columns => [ 'artist.name' ], join => [ 'artist' ] })->find(1);

is($cd->title, 'Spoonful of bees', 'Correct CD returned with include');
is($cd->get_column('name'), 'Caterwauler McCrae', 'Additional column returned');

# update_or_insert
$new = $schema->resultset("Track")->new( {
  trackid => 100,
  cd => 1,
  position => 4,
  title => 'Insert or Update',
  last_updated_on => '1973-07-19 12:01:02'
} );
$new->update_or_insert;
ok($new->in_storage, 'update_or_insert insert ok');

# test in update mode
$new->pos(5);
$new->update_or_insert;
is( $schema->resultset("Track")->find(100)->pos, 5, 'update_or_insert update ok');

# get_inflated_columns w/relation and accessor alias
SKIP: {
    skip "This test requires DateTime::Format::MySQL", 8 if $NO_DTFM;

    isa_ok($new->updated_date, 'DateTime', 'have inflated object via accessor');
    my %tdata = $new->get_inflated_columns;
    is($tdata{'trackid'}, 100, 'got id');
    isa_ok($tdata{'cd'}, 'DBICTest::CD', 'cd is CD object');
    is($tdata{'cd'}->id, 1, 'cd object is id 1');
    is($tdata{'position'}, 5, 'got position from pos');
    is($tdata{'title'}, 'Insert or Update');
    is($tdata{'last_updated_on'}, '1973-07-19T12:01:02');
    isa_ok($tdata{'last_updated_on'}, 'DateTime', 'inflated accessored column');
}

eval { $schema->class("Track")->load_components('DoesNotExist'); };

ok $@, $@;

is($schema->class("Artist")->field_name_for->{name}, 'artist name', 'mk_classdata usage ok');

my $search = [ { 'tags.tag' => 'Cheesy' }, { 'tags.tag' => 'Blue' } ];

my( $or_rs ) = $schema->resultset("CD")->search_rs($search, { join => 'tags',
                                                  order_by => 'cdid' });

cmp_ok($or_rs->count, '==', 5, 'Search with OR ok');

my $distinct_rs = $schema->resultset("CD")->search($search, { join => 'tags', distinct => 1 });
cmp_ok($distinct_rs->all, '==', 4, 'DISTINCT search with OR ok');

SKIP: {
  skip "SQLite < 3.2.6 doesn't understand COUNT(DISTINCT())", 1
    if $is_broken_sqlite;

  my $tcount = $schema->resultset("Track")->search(
    {},
    {       
       select => {count => {distinct => ['position', 'title']}},
	   as => ['count']
    }
  );
  cmp_ok($tcount->next->get_column('count'), '==', 13, 'multiple column COUNT DISTINCT ok');

}
my $tag_rs = $schema->resultset('Tag')->search(
               [ { 'me.tag' => 'Cheesy' }, { 'me.tag' => 'Blue' } ]);

my $rel_rs = $tag_rs->search_related('cd');

cmp_ok($rel_rs->count, '==', 5, 'Related search ok');

cmp_ok($or_rs->next->cdid, '==', $rel_rs->next->cdid, 'Related object ok');
$or_rs->reset;
$rel_rs->reset;

my $tag = $schema->resultset('Tag')->search(
               [ { 'me.tag' => 'Blue' } ], { cols=>[qw/tagid/] } )->next;

cmp_ok($tag->has_column_loaded('tagid'), '==', 1, 'Has tagid loaded');
cmp_ok($tag->has_column_loaded('tag'), '==', 0, 'Has not tag  loaded');

ok($schema->storage(), 'Storage available');

{
  my $rs = $schema->resultset("Artist")->search({
    -and => [
      artistid => { '>=', 1 },
      artistid => { '<', 3 }
    ]
  });

  $rs->update({ name => 'Test _cond_for_update_delete' });

  my $art;

  $art = $schema->resultset("Artist")->find(1);
  is($art->name, 'Test _cond_for_update_delete', 'updated first artist name');

  $art = $schema->resultset("Artist")->find(2);
  is($art->name, 'Test _cond_for_update_delete', 'updated second artist name');
}

# test source_name
{
  # source_name should be set for normal modules
  is($schema->source('CD')->source_name, 'CD', 'source_name is set to moniker');

  # test the result source that sets source_name explictly
  ok($schema->source('SourceNameArtists'), 'SourceNameArtists result source exists');

  my @artsn = $schema->resultset('SourceNameArtists')->search({}, { order_by => 'name DESC' });
  cmp_ok(@artsn, '==', 4, "Four artists returned");
  
  # make sure subclasses that don't set source_name are ok
  ok($schema->source('ArtistSubclass'), 'ArtistSubclass exists');
}

my $newbook = $schema->resultset( 'Bookmark' )->find(1);

$@ = '';
eval {
my $newlink = $newbook->link;
};
ok(!$@, "stringify to false value doesn't cause error");

# test cascade_delete through many_to_many relations
{
  my $art_del = $schema->resultset("Artist")->find({ artistid => 1 });
  $art_del->delete;
  cmp_ok( $schema->resultset("CD")->search({artist => 1}), '==', 0, 'Cascading through has_many top level.');
  cmp_ok( $schema->resultset("CD_to_Producer")->search({cd => 1}), '==', 0, 'Cascading through has_many children.');
}

# test column_info
{
  $schema->source("Artist")->{_columns}{'artistid'} = {};
  $schema->source("Artist")->column_info_from_storage(1);

  my $typeinfo = $schema->source("Artist")->column_info('artistid');
  is($typeinfo->{data_type}, 'INTEGER', 'column_info ok');
  $schema->source("Artist")->column_info('artistid');
  ok($schema->source("Artist")->{_columns_info_loaded} == 1, 'Columns info flag set');
}

# test source_info
{
  my $expected = {
    "source_info_key_A" => "source_info_value_A",
    "source_info_key_B" => "source_info_value_B",
    "source_info_key_C" => "source_info_value_C",
  };

  my $sinfo = $schema->source("Artist")->source_info;

  is_deeply($sinfo, $expected, 'source_info data works');
}

# test remove_columns
{
  is_deeply([$schema->source('CD')->columns], [qw/cdid artist title year/]);
  $schema->source('CD')->remove_columns('year');
  is_deeply([$schema->source('CD')->columns], [qw/cdid artist title/]);
  ok(! exists $schema->source('CD')->_columns->{'year'}, 'year still exists in _columns');
}

# test get_inflated_columns with objects
SKIP: {
    skip "This test requires DateTime::Format::MySQL", 5 if $NO_DTFM;
    my $event = $schema->resultset('Event')->search->first;
    my %edata = $event->get_inflated_columns;
    is($edata{'id'}, $event->id, 'got id');
    isa_ok($edata{'starts_at'}, 'DateTime', 'start_at is DateTime object');
    isa_ok($edata{'created_on'}, 'DateTime', 'create_on DateTime object');
    is($edata{'starts_at'}, $event->starts_at, 'got start date');
    is($edata{'created_on'}, $event->created_on, 'got created date');
}

# test resultsource->table return value when setting
{
    my $class = $schema->class('Event');
    diag $class;
    my $table = $class->table($class->table);
    is($table, $class->table, '->table($table) returns $table');
}
