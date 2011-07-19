use strict;
use warnings;

use Test::More;
use Test::Exception;

use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

lives_ok ( sub {
  my $no_prefetch = $schema->resultset('Track')->search_related(cd =>
    {
      'cd.year' => "2000",
    },
    {
      join => 'tags',
      order_by => 'me.trackid',
      rows => 1,
    }
  );

  my $use_prefetch = $no_prefetch->search(
    {},
    {
      prefetch => 'tags',
    }
  );

  is($use_prefetch->count, $no_prefetch->count, 'counts with and without prefetch match');
  is(
    scalar ($use_prefetch->all),
    scalar ($no_prefetch->all),
    "Amount of returned rows is right"
  );

}, 'search_related prefetch with order_by works');

TODO: { local $TODO = 'Unqualified columns in where clauses can not be fixed without an SQLA rewrite' if SQL::Abstract->VERSION < 2;
lives_ok ( sub {
  my $no_prefetch = $schema->resultset('Track')->search_related(cd =>
    {
      'cd.year' => "2000",
      'tagid' => 1,
    },
    {
      join => 'tags',
      rows => 1,
    }
  );

  my $use_prefetch = $no_prefetch->search(
    undef,
    {
      prefetch => 'tags',
    }
  );

  is(
    scalar ($use_prefetch->all),
    scalar ($no_prefetch->all),
    "Amount of returned rows is right"
  );
  is($use_prefetch->count, $no_prefetch->count, 'counts with and without prefetch match');

}, 'search_related prefetch with condition referencing unqualified column of a joined table works');
}


lives_ok (sub {
    my $rs = $schema->resultset("Artwork")->search(undef, {distinct => 1})
              ->search_related('artwork_to_artist')->search_related('artist',
                undef,
                { prefetch => 'cds' },
              );
    is($rs->all, 0, 'prefetch without WHERE (objects)');
    is($rs->count, 0, 'prefetch without WHERE (count)');

    $rs = $schema->resultset("Artwork")->search(undef, {distinct => 1})
              ->search_related('artwork_to_artist')->search_related('artist',
                { 'cds.title' => 'foo' },
                { prefetch => 'cds' },
              );
    is($rs->all, 0, 'prefetch with WHERE (objects)');
    is($rs->count, 0, 'prefetch with WHERE (count)');


# test where conditions at the root of the related chain
    my $artist_rs = $schema->resultset("Artist")->search({artistid => 2});
    my $artist = $artist_rs->next;
    $artist->create_related ('cds', $_) for (
      {
        year => 1999, title => 'vague cd', genre => { name => 'vague genre' }
      },
      {
        year => 1999, title => 'vague cd2', genre => { name => 'vague genre' }
      },
    );

    $rs = $artist_rs->search_related('cds')->search_related('genre',
                    { 'genre.name' => 'vague genre' },
                    { prefetch => 'cds' },
                 );
    is($rs->all, 1, 'base without distinct (objects)');
    is($rs->count, 1, 'base without distinct (count)');
    # artist -> 2 cds -> 2 genres -> 2 cds for each genre = 4
    is($rs->search_related('cds')->all, 4, 'prefetch without distinct (objects)');
    is($rs->search_related('cds')->count, 4, 'prefetch without distinct (count)');


    $rs = $artist_rs->search_related('cds', {}, { distinct => 1})->search_related('genre',
                    { 'genre.name' => 'vague genre' },
                 );
    is($rs->all, 2, 'distinct does not propagate over search_related (objects)');
    is($rs->count, 2, 'distinct does not propagate over search_related (count)');

    $rs = $rs->search ({}, { distinct => 1} );
    is($rs->all, 1, 'distinct without prefetch (objects)');
    is($rs->count, 1, 'distinct without prefetch (count)');


    $rs = $artist_rs->search_related('cds')->search_related('genre',
                    { 'genre.name' => 'vague genre' },
                    { prefetch => 'cds', distinct => 1 },
                 );
    is($rs->all, 1, 'distinct with prefetch (objects)');
    is($rs->count, 1, 'distinct with prefetch (count)');

  TODO: {
    local $TODO = "This makes another 2 trips to the database, it can't be right";
    # artist -> 2 cds -> 2 genres -> 2 cds for each genre + distinct = 2
    is($rs->search_related('cds')->all, 2, 'prefetched distinct with prefetch (objects)');
    is($rs->search_related('cds')->count, 2, 'prefetched distinct with prefetch (count)');
  }

}, 'distinct generally works with prefetch on deep search_related chains');

done_testing;
