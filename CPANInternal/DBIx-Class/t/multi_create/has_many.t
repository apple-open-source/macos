use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan tests => 2;

my $schema = DBICTest->init_schema();

my $track_no_lyrics = $schema->resultset ('Track')
              ->search ({ 'lyrics.lyric_id' => undef }, { join => 'lyrics' })
                ->first;

my $lyric = $track_no_lyrics->create_related ('lyrics', {
  lyric_versions => [
    { text => 'english doubled' },
    { text => 'english doubled' },
  ],
});
is ($lyric->lyric_versions->count, 2, "Two identical has_many's created");


my $link = $schema->resultset ('Link')->create ({
  url => 'lolcats!',
  bookmarks => [
    {},
    {},
  ]
});
is ($link->bookmarks->count, 2, "Two identical default-insert has_many's created");
