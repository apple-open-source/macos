use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan 'no_plan';

my $schema = DBICTest->init_schema();

my $query_stats;
$schema->storage->debugcb (sub { push @{$query_stats->{$_[0]}}, $_[1] });
$schema->storage->debug (1);

TODO: {
  local $TODO = 'This is an optimization task, will wait... a while';

lives_ok (sub {
  undef $query_stats;
  $schema->resultset('Artist')->create ({
    name => 'poor artist',
    cds => [
      {
        title => 'cd1',
        year => 2001,
      },
      {
        title => 'cd2',
        year => 2002,
      },
    ],
  });

  is ( @{$query_stats->{INSERT} || []}, 3, 'number of inserts during creation of artist with 2 cds' );
  is ( @{$query_stats->{SELECT} || []}, 0, 'number of selects during creation of artist with 2 cds' )
    || $ENV{DBIC_MULTICREATE_DEBUG} && diag join "\n", @{$query_stats->{SELECT} || []};
});


lives_ok (sub {
  undef $query_stats;
  $schema->resultset('Artist')->create ({
    name => 'poorer artist',
    cds => [
      {
        title => 'cd3',
        year => 2003,
        genre => { name => 'vague genre' },
      },
      {
        title => 'cd4',
        year => 2004,
        genre => { name => 'vague genre' },
      },
    ],
  });

  is ( @{$query_stats->{INSERT} || []}, 4, 'number of inserts during creation of artist with 2 cds, converging on the same genre' );
  is ( @{$query_stats->{SELECT} || []}, 0, 'number of selects during creation of artist with 2 cds, converging on the same genre' )
    || $ENV{DBIC_MULTICREATE_DEBUG} && diag join "\n", @{$query_stats->{SELECT} || []};
});


lives_ok (sub {
  my $genre = $schema->resultset('Genre')->first;
  undef $query_stats;
  $schema->resultset('Artist')->create ({
    name => 'poorest artist',
    cds => [
      {
        title => 'cd5',
        year => 2005,
        genre => $genre,
      },
      {
        title => 'cd6',
        year => 2004,
        genre => $genre,
      },
    ],
  });

  is ( @{$query_stats->{INSERT} || []}, 3, 'number of inserts during creation of artist with 2 cds, converging on the same existing genre' );
  is ( @{$query_stats->{SELECT} || []}, 0, 'number of selects during creation of artist with 2 cds, converging on the same existing genre' )
    || $ENV{DBIC_MULTICREATE_DEBUG} && diag join "\n", @{$query_stats->{SELECT} || []};
});


lives_ok (sub {
  undef $query_stats;
  $schema->resultset('Artist')->create ({
    name => 'poorer than the poorest artist',
    cds => [
      {
        title => 'cd7',
        year => 2007,
        cd_to_producer => [
          {
            producer => {
              name => 'jolly producer',
              producer_to_cd => [
                {
                  cd => {
                    title => 'cd8',
                    year => 2008,
                    artist => {
                      name => 'poorer than the poorest artist',
                    },
                  },
                },
              ],
            },
          },
        ],
      },
    ],
  });

  is ( @{$query_stats->{INSERT} || []}, 6, 'number of inserts during creation of artist->cd->producer->cd->same_artist' );
  is ( @{$query_stats->{SELECT} || []}, 0, 'number of selects during creation of artist->cd->producer->cd->same_artist' )
    || $ENV{DBIC_MULTICREATE_DEBUG} && diag join "\n", @{$query_stats->{SELECT} || []};
});

lives_ok (sub {
  undef $query_stats;
  $schema->resultset ('Artist')->find(1)->create_related (cds => {
    title => 'cd9',
    year => 2009,
    cd_to_producer => [
      {
        producer => {
          name => 'jolly producer',
          producer_to_cd => [
            {
              cd => {
                title => 'cd10',
                year => 2010,
                artist => {
                  name => 'poorer than the poorest artist',
                },
              },
            },
          ],
        },
      },
    ],
  });

  is ( @{$query_stats->{INSERT} || []}, 4, 'number of inserts during creation of existing_artist->cd->existing_producer->cd->existing_artist2' );
  is ( @{$query_stats->{SELECT} || []}, 0, 'number of selects during creation of existing_artist->cd->existing_producer->cd->existing_artist2' )
    || $ENV{DBIC_MULTICREATE_DEBUG} && diag join "\n", @{$query_stats->{SELECT} || []};
});

lives_ok (sub {
  undef $query_stats;

  my $artist = $schema->resultset ('Artist')->first;
  my $producer = $schema->resultset ('Producer')->first;

  $schema->resultset ('CD')->create ({
    title => 'cd11',
    year => 2011,
    artist => $artist,
    cd_to_producer => [
      {
        producer => $producer,
      },
    ],
  });

  is ( @{$query_stats->{INSERT} || []}, 2, 'number of inserts during creation of artist_object->cd->producer_object' );
  is ( @{$query_stats->{SELECT} || []}, 0, 'number of selects during creation of artist_object->cd->producer_object' )
    || $ENV{DBIC_MULTICREATE_DEBUG} && diag join "\n", @{$query_stats->{SELECT} || []};
});

}

1;
