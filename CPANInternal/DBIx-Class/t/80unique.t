use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;
use DBIC::DebugObj;

my $schema = DBICTest->init_schema();

# Check the defined unique constraints
is_deeply(
  [ sort $schema->source('CD')->unique_constraint_names ],
  [ qw/cd_artist_title primary/ ],
  'CD source has an automatically named unique constraint'
);
is_deeply(
  [ sort $schema->source('Producer')->unique_constraint_names ],
  [ qw/primary prod_name/ ],
  'Producer source has a named unique constraint'
);
is_deeply(
  [ sort $schema->source('Track')->unique_constraint_names ],
  [ qw/primary track_cd_position track_cd_title/ ],
  'Track source has three unique constraints'
);

my $artistid = 1;
my $title    = 'UNIQUE Constraint';

my $cd1 = $schema->resultset('CD')->find_or_create({
  artist => $artistid,
  title  => $title,
  year   => 2005,
});

my $cd2 = $schema->resultset('CD')->find(
  {
    artist => $artistid,
    title  => $title,
  },
  { key => 'cd_artist_title' }
);

is($cd2->get_column('artist'), $cd1->get_column('artist'), 'find by specific key: artist is correct');
is($cd2->title, $cd1->title, 'title is correct');
is($cd2->year, $cd1->year, 'year is correct');

my $cd3 = $schema->resultset('CD')->find($artistid, $title, { key => 'cd_artist_title' });

is($cd3->get_column('artist'), $cd1->get_column('artist'), 'find by specific key, ordered columns: artist is correct');
is($cd3->title, $cd1->title, 'title is correct');
is($cd3->year, $cd1->year, 'year is correct');

my $cd4 = $schema->resultset('CD')->update_or_create(
  {
    artist => $artistid,
    title  => $title,
    year   => 2007,
  },
);

ok(! $cd4->is_changed, 'update_or_create without key: row is clean');
is($cd4->cdid, $cd2->cdid, 'cdid is correct');
is($cd4->get_column('artist'), $cd2->get_column('artist'), 'artist is correct');
is($cd4->title, $cd2->title, 'title is correct');
is($cd4->year, 2007, 'updated year is correct');

my $cd5 = $schema->resultset('CD')->update_or_create(
  {
    artist => $artistid,
    title  => $title,
    year   => 2007,
  },
  { key => 'cd_artist_title' }
);

ok(! $cd5->is_changed, 'update_or_create by specific key: row is clean');
is($cd5->cdid, $cd2->cdid, 'cdid is correct');
is($cd5->get_column('artist'), $cd2->get_column('artist'), 'artist is correct');
is($cd5->title, $cd2->title, 'title is correct');
is($cd5->year, 2007, 'updated year is correct');

my $cd6 = $schema->resultset('CD')->update_or_create(
  {
    cdid   => $cd2->cdid,
    artist => 1,
    title  => $cd2->title,
    year   => 2005,
  },
  { key => 'primary' }
);

ok(! $cd6->is_changed, 'update_or_create by PK: row is clean');
is($cd6->cdid, $cd2->cdid, 'cdid is correct');
is($cd6->get_column('artist'), $cd2->get_column('artist'), 'artist is correct');
is($cd6->title, $cd2->title, 'title is correct');
is($cd6->year, 2005, 'updated year is correct');

my $cd7 = $schema->resultset('CD')->find_or_create(
  {
    artist => $artistid,
    title  => $title,
    year   => 2010,
  },
  { key => 'cd_artist_title' }
);

is($cd7->cdid, $cd1->cdid, 'find_or_create by specific key: cdid is correct');
is($cd7->get_column('artist'), $cd1->get_column('artist'), 'artist is correct');
is($cd7->title, $cd1->title, 'title is correct');
is($cd7->year, $cd1->year, 'year is correct');

my $artist = $schema->resultset('Artist')->find($artistid);
my $cd8 = $artist->find_or_create_related('cds',
  {
    title  => $title,
    year   => 2020,
  },
  { key => 'cd_artist_title' }
);

is($cd8->cdid, $cd1->cdid, 'find_or_create related by specific key: cdid is correct');
is($cd8->get_column('artist'), $cd1->get_column('artist'), 'artist is correct');
is($cd8->title, $cd1->title, 'title is correct');
is($cd8->year, $cd1->year, 'year is correct');

my $cd9 = $artist->cds->update_or_create(
  {
    cdid   => $cd1->cdid,
    title  => $title,
    year   => 2021,
  },
  { key => 'cd_artist_title' }
);

ok(! $cd9->is_changed, 'update_or_create by specific key: row is clean');
is($cd9->cdid, $cd1->cdid, 'cdid is correct');
is($cd9->get_column('artist'), $cd1->get_column('artist'), 'artist is correct');
is($cd9->title, $cd1->title, 'title is correct');
is($cd9->year, 2021, 'year is correct');

# Table with two unique constraints, and we're satisying one of them
my $track = $schema->resultset('Track')->find(
  {
    cd       => 1,
    position => 3,
  },
  { order_by => 'position' }
);

is($track->get_column('cd'), 1, 'track cd is correct');
is($track->get_column('position'), 3, 'track position is correct');

# Test a table with a unique constraint but no primary key
my $row = $schema->resultset('NoPrimaryKey')->update_or_create(
  {
    foo => 1,
    bar => 2,
    baz => 3,
  },
  { key => 'foo_bar' }
);

ok(! $row->is_changed, 'update_or_create on table without primary key: row is clean');
is($row->foo, 1, 'foo is correct');
is($row->bar, 2, 'bar is correct');
is($row->baz, 3, 'baz is correct');

# Test a unique condition with extra information in the where attr
{
  my $artist = $schema->resultset('Artist')->find({ artistid => 1 });
  my $cd = $artist->cds->find_or_new(
    {
      cdid  => 1,
      title => 'Not The Real Title',
      year  => 3000,
    },
    { key => 'primary' }
  );

  ok($cd->in_storage, 'find correctly grepped the key across a relationship');
  is($cd->cdid, 1, 'cdid is correct');
}

# Test update_or_new
{
    my $cd1 = $schema->resultset('CD')->update_or_new(
      {
        artist => $artistid,
        title  => "SuperHits $$",
        year   => 2007,
      },
      { key => 'cd_artist_title' }
    );

    is($cd1->in_storage, 0, 'CD is not in storage yet after update_or_new');
    $cd1->insert;
    ok($cd1->in_storage, 'CD got added to strage after update_or_new && insert');

    my $cd2 = $schema->resultset('CD')->update_or_new(
      {
        artist => $artistid,
        title  => "SuperHits $$",
        year   => 2008,
      },
      { key => 'cd_artist_title' }
    );
    ok($cd2->in_storage, 'Updating year using update_or_new was successful');
    is($cd2->id, $cd1->id, 'Got the same CD using update_or_new');
}

# make sure the ident condition is assembled sanely
{
  my $artist = $schema->resultset('Artist')->next;

  my ($sql, @bind);
  $schema->storage->debugobj(DBIC::DebugObj->new(\$sql, \@bind)),
  $schema->storage->debug(1);

  $artist->discard_changes;

  is_same_sql_bind (
    $sql,
    \@bind,
    'SELECT me.artistid, me.name, me.rank, me.charfield FROM artist me WHERE me.artistid = ?',
    [qw/'1'/],
  );

  $schema->storage->debug(0);
  $schema->storage->debugobj(undef);
}

done_testing;
