package # hide from PAUSE
    DBICTest::Schema::ArtistSubclass;

use base 'DBICTest::Schema::Artist';

__PACKAGE__->table(__PACKAGE__->table);

1;