use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 9;

my $artist = $schema->resultset ('Artist')->first;

my $genre = $schema->resultset ('Genre')
            ->create ({ name => 'par excellence' });

is ($genre->search_related( 'model_cd' )->count, 0, 'No cds yet');

# expect a create
$genre->update_or_create_related ('model_cd', {
  artist => $artist,
  year => 2009,
  title => 'the best thing since sliced bread',
});

# verify cd was inserted ok
is ($genre->search_related( 'model_cd' )->count, 1, 'One cd');
my $cd = $genre->find_related ('model_cd', {});
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
$genre->update_or_create_related ('model_cd', {
  year => 2010,
  title => 'the best thing since sliced bread',
});

# re-fetch the cd, verify update
is ($genre->search_related( 'model_cd' )->count, 1, 'Still one cd');
$cd = $genre->find_related ('model_cd', {});
is_deeply (
  { map { $_, $cd->get_column ($_) } qw/artist year title/ },
  {
    artist => $artist->id,
    year => 2010,
    title => 'the best thing since sliced bread',
  },
  'CD year column updated correctly',
);


# expect an update of the only related row
# (update a unique column)
$genre->update_or_create_related ('model_cd', {
  title => 'the best thing since vertical toasters',
});

# re-fetch the cd, verify update
is ($genre->search_related( 'model_cd' )->count, 1, 'Still one cd');
$cd = $genre->find_related ('model_cd', {});
is_deeply (
  { map { $_, $cd->get_column ($_) } qw/artist year title/ },
  {
    artist => $artist->id,
    year => 2010,
    title => 'the best thing since vertical toasters',
  },
  'CD title column updated correctly',
);


# expect a year update on the only related row
# (non-unique column only)
$genre->update_or_create_related ('model_cd', {
  year => 2011,
});

# re-fetch the cd, verify update
is ($genre->search_related( 'model_cd' )->count, 1, 'Still one cd');
$cd = $genre->find_related ('model_cd', {});
is_deeply (
  { map { $_, $cd->get_column ($_) } qw/artist year title/ },
  {
    artist => $artist->id,
    year => 2011,
    title => 'the best thing since vertical toasters',
  },
  'CD year column updated correctly without a disambiguator',
);
