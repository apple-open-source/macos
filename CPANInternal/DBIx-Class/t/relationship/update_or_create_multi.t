use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();
my $sdebug = $schema->storage->debug;

plan tests => 6;

my $artist = $schema->resultset ('Artist')->first;

my $genre = $schema->resultset ('Genre')
            ->create ({ name => 'par excellence' });

is ($genre->search_related( 'cds' )->count, 0, 'No cds yet');

# expect a create
$genre->update_or_create_related ('cds', {
  artist => $artist,
  year => 2009,
  title => 'the best thing since sliced bread',
});

# verify cd was inserted ok
is ($genre->search_related( 'cds' )->count, 1, 'One cd');
my $cd = $genre->find_related ('cds', {});
is_deeply (
  { map { $_, $cd->get_column ($_) } qw/artist year title/ },
  {
    artist => $artist->id,
    year => 2009,
    title => 'the best thing since sliced bread',
  },
  'CD created correctly',
);

# expect a year update on the only related row
# (non-qunique column + unique column as disambiguator)
$genre->update_or_create_related ('cds', {
  year => 2010,
  title => 'the best thing since sliced bread',
});

# re-fetch the cd, verify update
is ($genre->search_related( 'cds' )->count, 1, 'Still one cd');
$cd = $genre->find_related ('cds', {});
is_deeply (
  { map { $_, $cd->get_column ($_) } qw/artist year title/ },
  {
    artist => $artist->id,
    year => 2010,
    title => 'the best thing since sliced bread',
  },
  'CD year column updated correctly',
);


# expect a create, after a failed search using *only* the
# *current* relationship and the unique column constraints
# (so no year)
my @sql;
$schema->storage->debugcb(sub { push @sql, $_[1] });
$schema->storage->debug (1);

$genre->update_or_create_related ('cds', {
  title => 'the best thing since vertical toasters',
  artist => $artist,
  year => 2012,
});

$schema->storage->debugcb(undef);
$schema->storage->debug ($sdebug);

my ($search_sql) = $sql[0] =~ /^(SELECT .+?)\:/;
is_same_sql (
  $search_sql,
  'SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
    FROM cd me 
    WHERE ( me.artist = ? AND me.title = ? AND me.genreid = ? )
  ',
  'expected select issued',
);

# a has_many search without a unique constraint makes no sense
# but I am not sure what to test for - leaving open
