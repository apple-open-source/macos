use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use Data::Dumper;

my $schema = DBICTest->init_schema();

my $orig_debug = $schema->storage->debug;

use IO::File;

BEGIN {
    eval "use DBD::SQLite";
    plan $@
        ? ( skip_all => 'needs DBD::SQLite for testing' )
        : ( tests => 64 );
}

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

# test the abstract join => SQL generator
my $sa = new DBIC::SQL::Abstract;

my @j = (
    { child => 'person' },
    [ { father => 'person' }, { 'father.person_id' => 'child.father_id' }, ],
    [ { mother => 'person' }, { 'mother.person_id' => 'child.mother_id' } ],
);
my $match = 'person child JOIN person father ON ( father.person_id = '
          . 'child.father_id ) JOIN person mother ON ( mother.person_id '
          . '= child.mother_id )'
          ;
is( $sa->_recurse_from(@j), $match, 'join 1 ok' );

my @j2 = (
    { mother => 'person' },
    [   [   { child => 'person' },
            [   { father             => 'person' },
                { 'father.person_id' => 'child.father_id' }
            ]
        ],
        { 'mother.person_id' => 'child.mother_id' }
    ],
);
$match = 'person mother JOIN (person child JOIN person father ON ('
       . ' father.person_id = child.father_id )) ON ( mother.person_id = '
       . 'child.mother_id )'
       ;
is( $sa->_recurse_from(@j2), $match, 'join 2 ok' );

my @j3 = (
    { child => 'person' },
    [ { father => 'person', -join_type => 'inner' }, { 'father.person_id' => 'child.father_id' }, ],
    [ { mother => 'person', -join_type => 'inner'  }, { 'mother.person_id' => 'child.mother_id' } ],
);
$match = 'person child INNER JOIN person father ON ( father.person_id = '
          . 'child.father_id ) INNER JOIN person mother ON ( mother.person_id '
          . '= child.mother_id )'
          ;

is( $sa->_recurse_from(@j3), $match, 'join 3 (inner join) ok');

my @j4 = (
    { mother => 'person' },
    [   [   { child => 'person', -join_type => 'left' },
            [   { father             => 'person', -join_type => 'right' },
                { 'father.person_id' => 'child.father_id' }
            ]
        ],
        { 'mother.person_id' => 'child.mother_id' }
    ],
);
$match = 'person mother LEFT JOIN (person child RIGHT JOIN person father ON ('
       . ' father.person_id = child.father_id )) ON ( mother.person_id = '
       . 'child.mother_id )'
       ;
is( $sa->_recurse_from(@j4), $match, 'join 4 (nested joins + join types) ok');

my @j5 = (
    { child => 'person' },
    [ { father => 'person' }, { 'father.person_id' => \'!= child.father_id' }, ],
    [ { mother => 'person' }, { 'mother.person_id' => 'child.mother_id' } ],
);
$match = 'person child JOIN person father ON ( father.person_id != '
          . 'child.father_id ) JOIN person mother ON ( mother.person_id '
          . '= child.mother_id )'
          ;
is( $sa->_recurse_from(@j5), $match, 'join 5 (SCALAR reference for ON statement) ok' );

my @j6 = (
    { child => 'person' },
    [ { father => 'person' }, { 'father.person_id' => { '!=', '42' } }, ],
    [ { mother => 'person' }, { 'mother.person_id' => 'child.mother_id' } ],
);
$match = qr/^HASH reference arguments are not supported in JOINS - try using "\.\.\." instead/;
eval { $sa->_recurse_from(@j6) };
like( $@, $match, 'join 6 (HASH reference for ON statement dies) ok' );

my $rs = $schema->resultset("CD")->search(
           { 'year' => 2001, 'artist.name' => 'Caterwauler McCrae' },
           { from => [ { 'me' => 'cd' },
                         [
                           { artist => 'artist' },
                           { 'me.artist' => 'artist.artistid' }
                         ] ] }
         );

cmp_ok( $rs + 0, '==', 1, "Single record in resultset");

is($rs->first->title, 'Forkful of bees', 'Correct record returned');

$rs = $schema->resultset("CD")->search(
           { 'year' => 2001, 'artist.name' => 'Caterwauler McCrae' },
           { join => 'artist' });

cmp_ok( $rs + 0, '==', 1, "Single record in resultset");

is($rs->first->title, 'Forkful of bees', 'Correct record returned');

$rs = $schema->resultset("CD")->search(
           { 'artist.name' => 'We Are Goth',
             'liner_notes.notes' => 'Kill Yourself!' },
           { join => [ qw/artist liner_notes/ ] });

cmp_ok( $rs + 0, '==', 1, "Single record in resultset");

is($rs->first->title, 'Come Be Depressed With Us', 'Correct record returned');

# when using join attribute, make sure slice()ing all objects has same count as all()
$rs = $schema->resultset("CD")->search(
    { 'artist' => 1 },
    { join => [qw/artist/], order_by => 'artist.name' }
);
cmp_ok( scalar $rs->all, '==', scalar $rs->slice(0, $rs->count - 1), 'slice() with join has same count as all()' );

ok(!$rs->slice($rs->count+1000, $rs->count+1002)->count,
  'Slicing beyond end of rs returns a zero count');

$rs = $schema->resultset("Artist")->search(
        { 'liner_notes.notes' => 'Kill Yourself!' },
        { join => { 'cds' => 'liner_notes' } });

cmp_ok( $rs->count, '==', 1, "Single record in resultset");

is($rs->first->name, 'We Are Goth', 'Correct record returned');

# bug in 0.07000 caused attr (join/prefetch) to be modifed by search
# so we check the search & attr arrays are not modified
my $search = { 'artist.name' => 'Caterwauler McCrae' };
my $attr = { prefetch => [ qw/artist liner_notes/ ],
             order_by => 'me.cdid' };
my $search_str = Dumper($search);
my $attr_str = Dumper($attr);

$rs = $schema->resultset("CD")->search($search, $attr);

is(Dumper($search), $search_str, 'Search hash untouched after search()');
is(Dumper($attr), $attr_str, 'Attribute hash untouched after search()');
cmp_ok($rs + 0, '==', 3, 'Correct number of records returned');

my $queries = 0;
$schema->storage->debugcb(sub { $queries++; });
$schema->storage->debug(1);

my @cd = $rs->all;

is($cd[0]->title, 'Spoonful of bees', 'First record returned ok');

ok(!defined $cd[0]->liner_notes, 'No prefetch for NULL LEFT join');

is($cd[1]->{_relationship_data}{liner_notes}->notes, 'Buy Whiskey!', 'Prefetch for present LEFT JOIN');

is(ref $cd[1]->liner_notes, 'DBICTest::LinerNotes', 'Prefetch returns correct class');

is($cd[2]->{_inflated_column}{artist}->name, 'Caterwauler McCrae', 'Prefetch on parent object ok');

is($queries, 1, 'prefetch ran only 1 select statement');

$schema->storage->debug($orig_debug);
$schema->storage->debugobj->callback(undef);

# test for partial prefetch via columns attr
my $cd = $schema->resultset('CD')->find(1,
    {
      columns => [qw/title artist.name/], 
      join => { 'artist' => {} }
    }
);
ok(eval { $cd->artist->name eq 'Caterwauler McCrae' }, 'single related column prefetched');

# start test for nested prefetch SELECT count
$queries = 0;
$schema->storage->debugcb(sub { $queries++ });
$schema->storage->debug(1);

$rs = $schema->resultset('Tag')->search(
  {},
  {
    prefetch => { cd => 'artist' }
  }
);

my $tag = $rs->first;

is( $tag->cd->title, 'Spoonful of bees', 'step 1 ok for nested prefetch' );

is( $tag->cd->artist->name, 'Caterwauler McCrae', 'step 2 ok for nested prefetch');

# count the SELECTs
#$selects++ if /SELECT(?!.*WHERE 1=0.*)/;
is($queries, 1, 'nested prefetch ran exactly 1 select statement (excluding column_info)');

$queries = 0;

is($tag->search_related('cd')->search_related('artist')->first->name,
   'Caterwauler McCrae',
   'chained belongs_to->belongs_to search_related ok');

is($queries, 0, 'chained search_related after belontgs_to->belongs_to prefetch ran no queries');

$queries = 0;

$cd = $schema->resultset('CD')->find(1, { prefetch => 'artist' });

is($cd->{_inflated_column}{artist}->name, 'Caterwauler McCrae', 'artist prefetched correctly on find');

is($queries, 1, 'find with prefetch ran exactly 1 select statement (excluding column_info)');

$queries = 0;

$schema->storage->debugcb(sub { $queries++; });

$cd = $schema->resultset('CD')->find(1, { prefetch => { cd_to_producer => 'producer' } });

is($cd->producers->first->name, 'Matt S Trout', 'many_to_many accessor ok');

is($queries, 1, 'many_to_many accessor with nested prefetch ran exactly 1 query');

$queries = 0;

my $producers = $cd->search_related('cd_to_producer')->search_related('producer');

is($producers->first->name, 'Matt S Trout', 'chained many_to_many search_related ok');

is($queries, 0, 'chained search_related after many_to_many prefetch ran no queries');

$schema->storage->debug($orig_debug);
$schema->storage->debugobj->callback(undef);

$rs = $schema->resultset('Tag')->search(
  {},
  {
    join => { cd => 'artist' },
    prefetch => { cd => 'artist' }
  }
);

cmp_ok( $rs->count, '>=', 0, 'nested prefetch does not duplicate joins' );

my ($artist) = $schema->resultset("Artist")->search({ 'cds.year' => 2001 },
                 { order_by => 'artistid DESC', join => 'cds' });

is($artist->name, 'Random Boy Band', "Join search by object ok");

my @cds = $schema->resultset("CD")->search({ 'liner_notes.notes' => 'Buy Merch!' },
                               { join => 'liner_notes' });

cmp_ok(scalar @cds, '==', 1, "Single CD retrieved via might_have");

is($cds[0]->title, "Generic Manufactured Singles", "Correct CD retrieved");

my @artists = $schema->resultset("Artist")->search({ 'tags.tag' => 'Shiny' },
                                       { join => { 'cds' => 'tags' } });

cmp_ok( @artists, '==', 2, "two-join search ok" );

$rs = $schema->resultset("CD")->search(
  {},
  { group_by => [qw/ title me.cdid /] }
);

SKIP: {
    skip "SQLite < 3.2.6 doesn't understand COUNT(DISTINCT())", 1
        if $is_broken_sqlite;
    cmp_ok( $rs->count, '==', 5, "count() ok after group_by on main pk" );
}

cmp_ok( scalar $rs->all, '==', 5, "all() returns same count as count() after group_by on main pk" );

$rs = $schema->resultset("CD")->search(
  {},
  { join => [qw/ artist /], group_by => [qw/ artist.name /] }
);

SKIP: {
    skip "SQLite < 3.2.6 doesn't understand COUNT(DISTINCT())", 1
        if $is_broken_sqlite;
    cmp_ok( $rs->count, '==', 3, "count() ok after group_by on related column" );
}

$rs = $schema->resultset("Artist")->search(
  {},
      { join => [qw/ cds /], group_by => [qw/ me.name /], having =>{ 'MAX(cds.cdid)'=> \'< 5' } }
);

cmp_ok( $rs->all, '==', 2, "results ok after group_by on related column with a having" );

$rs = $rs->search( undef, {  having =>{ 'count(*)'=> \'> 2' }});

cmp_ok( $rs->all, '==', 1, "count() ok after group_by on related column with a having" );

$rs = $schema->resultset("Artist")->search(
        { 'cds.title' => 'Spoonful of bees',
          'cds_2.title' => 'Forkful of bees' },
        { join => [ 'cds', 'cds' ] });

SKIP: {
    skip "SQLite < 3.2.6 doesn't understand COUNT(DISTINCT())", 1
        if $is_broken_sqlite;
    cmp_ok($rs->count, '==', 1, "single artist returned from multi-join");
}

is($rs->next->name, 'Caterwauler McCrae', "Correct artist returned");

$cd = $schema->resultset('Artist')->first->create_related('cds',
    {
    title   => 'Unproduced Single',
    year    => 2007
});

my $left_join = $schema->resultset('CD')->search(
    { 'me.cdid' => $cd->cdid },
    { prefetch => { cd_to_producer => 'producer' } }
);

cmp_ok($left_join, '==', 1, 'prefetch with no join record present');

$queries = 0;
$schema->storage->debugcb(sub { $queries++ });
$schema->storage->debug(1);

my $tree_like =
     $schema->resultset('TreeLike')->find(4,
       { join     => { parent => { parent => 'parent' } },
         prefetch => { parent => { parent => 'parent' } } });

is($tree_like->name, 'quux', 'Bottom of tree ok');
$tree_like = $tree_like->parent;
is($tree_like->name, 'baz', 'First level up ok');
$tree_like = $tree_like->parent;
is($tree_like->name, 'bar', 'Second level up ok');
$tree_like = $tree_like->parent;
is($tree_like->name, 'foo', 'Third level up ok');

$schema->storage->debug($orig_debug);
$schema->storage->debugobj->callback(undef);

cmp_ok($queries, '==', 1, 'Only one query run');

$tree_like = $schema->resultset('TreeLike')->search({'me.id' => 1});
$tree_like = $tree_like->search_related('children')->search_related('children')->search_related('children')->first;
is($tree_like->name, 'quux', 'Tree search_related ok');

$tree_like = $schema->resultset('TreeLike')->search_related('children',
    { 'children.id' => 2, 'children_2.id' => 3 },
    { prefetch => { children => 'children' } }
  )->first;
is(eval { $tree_like->children->first->children->first->name }, 'quux',
   'Tree search_related with prefetch ok');

$tree_like = eval { $schema->resultset('TreeLike')->search(
    { 'children.id' => 2, 'children_2.id' => 5 }, 
    { join => [qw/children children/] }
  )->search_related('children', { 'children_4.id' => 6 }, { prefetch => 'children' }
  )->first->children->first; };
is(eval { $tree_like->name }, 'fong', 'Tree with multiple has_many joins ok');

# test that collapsed joins don't get a _2 appended to the alias

my $sql = '';
$schema->storage->debugcb(sub { $sql = $_[1] });
$schema->storage->debug(1);

eval {
  my $row = $schema->resultset('Artist')->search_related('cds', undef, {
    join => 'tracks',
    prefetch => 'tracks',
  })->search_related('tracks')->first;
};

like( $sql, qr/^SELECT tracks_2\.trackid/, "join not collapsed for search_related" );

$schema->storage->debug($orig_debug);
$schema->storage->debugobj->callback(undef);

$rs = $schema->resultset('Artist');
$rs->create({ artistid => 4, name => 'Unknown singer-songwriter' });
$rs->create({ artistid => 5, name => 'Emo 4ever' });
@artists = $rs->search(undef, { prefetch => 'cds', order_by => 'artistid' });
is(scalar @artists, 5, 'has_many prefetch with adjacent empty rows ok');

# -------------
#
# Tests for multilevel has_many prefetch

# artist resultsets - with and without prefetch
my $art_rs = $schema->resultset('Artist');
my $art_rs_pr = $art_rs->search(
    {},
    {
        join     => [ { cds => ['tracks'] } ],
        prefetch => [ { cds => ['tracks'] } ],
        cache    => 1 # last test needs this
    }
);

# This test does the same operation twice - once on a
# set of items fetched from the db with no prefetch of has_many rels
# The second prefetches 2 levels of has_many
# We check things are the same by comparing the name or title
# we build everything into a hash structure and compare the one
# from each rs to see what differs

sub make_hash_struc {
    my $rs = shift;

    my $struc = {};
    foreach my $art ( $rs->all ) {
        foreach my $cd ( $art->cds ) {
            foreach my $track ( $cd->tracks ) {
                $struc->{ $art->name }{ $cd->title }{ $track->title }++;
            }
        }
    }
    return $struc;
}

$queries = 0;
$schema->storage->debugcb(sub { $queries++ });
$schema->storage->debug(1);

my $prefetch_result = make_hash_struc($art_rs_pr);

is($queries, 1, 'nested prefetch across has_many->has_many ran exactly 1 query');

my $nonpre_result   = make_hash_struc($art_rs);

is_deeply( $prefetch_result, $nonpre_result,
    'Compare 2 level prefetch result to non-prefetch result' );

$queries = 0;

is($art_rs_pr->search_related('cds')->search_related('tracks')->first->title,
   'Fowlin',
   'chained has_many->has_many search_related ok'
  );

is($queries, 0, 'chained search_related after has_many->has_many prefetch ran no queries');
