# Test if prefetch and join in diamond relationship fetching the correct rows
use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

$schema->populate('Artwork', [
    [ qw/cd_id/ ],
    [ 1 ],
]);

$schema->populate('Artwork_to_Artist', [
    [ qw/artwork_cd_id artist_id/ ],
    [ 1, 2 ],
]);

my $ars = $schema->resultset ('Artwork');

# The relationship diagram here is:
#
#  $ars --> artwork_to_artist
#   |              |
#   |              |
#   V              V
#   cd  ------>  artist
#
# The current artwork belongs to a cd by artist1
# but the artwork itself is painted by artist2
#
# What we try is all possible permutations of join/prefetch 
# combinations in both directions, while always expecting to
# arrive at the specific artist at the end of each path.


my $cd_paths = {
  'no cd' => [],
  'cd' => ['cd'],
  'cd->artist1' => [{'cd' => 'artist'}]
};
my $a2a_paths = {
  'no a2a' => [],
  'a2a' => ['artwork_to_artist'],
  'a2a->artist2' => [{'artwork_to_artist' => 'artist'}]
};

my %tests;

foreach my $cd_path (keys %$cd_paths) {

  foreach my $a2a_path (keys %$a2a_paths) {


    $tests{sprintf "join %s, %s", $cd_path, $a2a_path} = $ars->search({}, {
      'join' => [
        @{ $cd_paths->{$cd_path} },
        @{ $a2a_paths->{$a2a_path} },
      ],
      'prefetch' => [
      ],
    });


    $tests{sprintf "prefetch %s, %s", $cd_path, $a2a_path} = $ars->search({}, {
      'join' => [
      ],
      'prefetch' => [
        @{ $cd_paths->{$cd_path} },
        @{ $a2a_paths->{$a2a_path} },
      ],
    });


    $tests{sprintf "join %s, prefetch %s", $cd_path, $a2a_path} = $ars->search({}, {
      'join' => [
        @{ $cd_paths->{$cd_path} },
      ],
      'prefetch' => [
        @{ $a2a_paths->{$a2a_path} },
      ],
    });


    $tests{sprintf "join %s, prefetch %s", $a2a_path, $cd_path} = $ars->search({}, {
      'join' => [
        @{ $a2a_paths->{$a2a_path} },
      ],
      'prefetch' => [
        @{ $cd_paths->{$cd_path} },
      ],
    });

  }
}

foreach my $name (keys %tests) {
  foreach my $artwork ($tests{$name}->all()) {
    is($artwork->id, 1, $name . ', correct artwork');
    is($artwork->cd->artist->artistid, 1, $name . ', correct artist_id over cd');
    is($artwork->artwork_to_artist->first->artist->artistid, 2, $name . ', correct artist_id over A2A');
  }
}

done_testing;
