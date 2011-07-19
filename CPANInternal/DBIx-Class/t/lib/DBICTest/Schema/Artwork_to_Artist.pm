package # hide from PAUSE
    DBICTest::Schema::Artwork_to_Artist;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('artwork_to_artist');
__PACKAGE__->add_columns(
  'artwork_cd_id' => {
    data_type => 'integer',
    is_foreign_key => 1,
  },
  'artist_id' => {
    data_type => 'integer',
    is_foreign_key => 1,
  },
);
__PACKAGE__->set_primary_key(qw/artwork_cd_id artist_id/);
__PACKAGE__->belongs_to('artwork', 'DBICTest::Schema::Artwork', 'artwork_cd_id');
__PACKAGE__->belongs_to('artist', 'DBICTest::Schema::Artist', 'artist_id');

1;
