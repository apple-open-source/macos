package # hide from PAUSE
    DBICTest::Schema::ArtistSourceName;

use base 'DBICTest::Schema::Artist';

__PACKAGE__->source_name('SourceNameArtists');

1;
