use strict;
use warnings;

use Test::More;
use Test::Exception;

use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();

my $new_rs = $schema->resultset('Artist')->search({
   'artwork_to_artist.artist_id' => 1
}, {
   join => 'artwork_to_artist'
});
lives_ok { $new_rs->count } 'regular search works';
lives_ok { $new_rs->search({ 'artwork_to_artist.artwork_cd_id' => 1})->count }
   '... and chaining off that using join works';
lives_ok { $new_rs->search({ 'artwork_to_artist.artwork_cd_id' => 1})->as_subselect_rs->count }
   '... and chaining off the virtual view works';
dies_ok  { $new_rs->as_subselect_rs->search({'artwork_to_artist.artwork_cd_id'=> 1})->count }
   q{... but chaining off of a virtual view using join doesn't work};
done_testing;
