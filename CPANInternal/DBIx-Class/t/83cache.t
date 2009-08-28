use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $queries;
$schema->storage->debugcb( sub{ $queries++ } );

eval "use DBD::SQLite";
plan skip_all => 'needs DBD::SQLite for testing' if $@;
plan tests => 23;

my $rs = $schema->resultset("Artist")->search(
  { artistid => 1 }
);

my $artist = $rs->first;

ok( !defined($rs->get_cache), 'cache is not populated without cache attribute' );

$rs = $schema->resultset('Artist')->search( undef, { cache => 1 } );
my $artists = [ $rs->all ];

is( scalar @{$rs->get_cache}, 3, 'all() populates cache for search with cache attribute' );

$rs->clear_cache;

ok( !defined($rs->get_cache), 'clear_cache is functional' );

$rs->next;

is( scalar @{$rs->get_cache}, 3, 'next() populates cache for search with cache attribute' );

pop( @$artists );
$rs->set_cache( $artists );

is( scalar @{$rs->get_cache}, 2, 'set_cache() is functional' );

my $cd = $schema->resultset('CD')->find(1);

$rs->clear_cache;

$queries = 0;
$schema->storage->debug(1);

$rs = $schema->resultset('Artist')->search( undef, { cache => 1 } );
while( $artist = $rs->next ) {}
$artist = $rs->first();

is( $queries, 1, 'revisiting a row does not issue a query when cache => 1' );

$schema->storage->debug(0);

my @a = $schema->resultset("Artist")->search(
  { },
  {
    join => [ qw/ cds /],
    prefetch => [qw/ cds /],
  }
);

is(scalar @a, 3, 'artist with cds: count parent objects');

$rs = $schema->resultset("Artist")->search(
  { 'artistid' => 1 },
  {
    join => [ qw/ cds /],
    prefetch => [qw/ cds /],
  }
);

use Data::Dumper; $Data::Dumper::Deparse = 1;

# start test for prefetch SELECT count
$queries = 0;
$schema->storage->debug(1);

$artist = $rs->first;
$rs->reset();

# make sure artist contains a related resultset for cds
is( ref $artist->{related_resultsets}->{cds}, 'DBIx::Class::ResultSet', 'artist has a related_resultset for cds' );

# check if $artist->cds->get_cache is populated
is( scalar @{$artist->cds->get_cache}, 3, 'cache for artist->cds contains correct number of records');

# ensure that $artist->cds returns correct number of objects
is( scalar ($artist->cds), 3, 'artist->cds returns correct number of objects' );

# ensure that $artist->cds->count returns correct value
is( $artist->cds->count, 3, 'artist->cds->count returns correct value' );

# ensure that $artist->count_related('cds') returns correct value
is( $artist->count_related('cds'), 3, 'artist->count_related returns correct value' );

is($queries, 1, 'only one SQL statement executed');

$schema->storage->debug(0);

# make sure related_resultset is deleted after object is updated
$artist->set_column('name', 'New Name');
$artist->update();

is( scalar keys %{$artist->{related_resultsets}}, 0, 'related resultsets deleted after update' );

# todo: make sure caching works with nested prefetch e.g. $artist->cds->tracks
$rs = $schema->resultset("Artist")->search(
  { artistid => 1 },
  {
    join => { cds => 'tags' },
    prefetch => {
      cds => 'tags'
    },
  }
);
{
my $artist_count_before = $schema->resultset('Artist')->count;
$schema->resultset("Artist")->create({artistid=>4,name=>qq{Humoungous Hamsters}});
is($schema->resultset('Artist')->count, $artist_count_before + 1, 'count() reflects new artist');
my $artist = $schema->resultset("Artist")->search(
  { artistid => 4 },{prefetch=>[qw/cds/]}
)->first;

is($artist->cds, 0, 'No cds for this artist');
}

# SELECT count for nested has_many prefetch
$queries = 0;
$schema->storage->debug(1);

$artist = ($rs->all)[0];

is($queries, 1, 'only one SQL statement executed');

$schema->storage->debug(0);

my @objs;
#$artist = $rs->find(1);

$queries = 0;
$schema->storage->debug(1);

my $cds = $artist->cds;
my $tags = $cds->next->tags;
while( my $tag = $tags->next ) {
  push @objs, $tag->tagid; #warn "tag:", $tag->ID, " => ", $tag->tag;
}

is_deeply( \@objs, [ 3 ], 'first cd has correct tags' );

$tags = $cds->next->tags;
@objs = ();
while( my $tag = $tags->next ) {
  push @objs, $tag->id; #warn "tag: ", $tag->ID;
}

is_deeply( \@objs, [ 1 ], 'second cd has correct tags' );

$tags = $cds->next->tags;
@objs = ();
while( my $tag = $tags->next ) {
  push @objs, $tag->id; #warn "tag: ", $tag->ID;
}

is_deeply( \@objs, [ 2, 5, 8 ], 'third cd has correct tags' );

is( $queries, 0, 'no additional SQL statements while checking nested data' );

# start test for prefetch SELECT count
$queries = 0;

$artist = $schema->resultset('Artist')->find(1, { prefetch => [qw/cds/] });

is( $queries, 1, 'only one select statement on find with inline has_many prefetch' );

# start test for prefetch SELECT count
$queries = 0;

$rs = $schema->resultset('Artist')->search(undef, { prefetch => [qw/cds/] });
$artist = $rs->find(1);

is( $queries, 1, 'only one select statement on find with has_many prefetch on resultset' );

$schema->storage->debug(0);

