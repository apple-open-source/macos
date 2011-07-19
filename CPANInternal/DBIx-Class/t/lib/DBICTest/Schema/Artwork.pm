package # hide from PAUSE
    DBICTest::Schema::Artwork;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('cd_artwork');
__PACKAGE__->add_columns(
  'cd_id' => {
    data_type => 'integer',
    is_nullable => 0,
  },
);
__PACKAGE__->set_primary_key('cd_id');
__PACKAGE__->belongs_to('cd', 'DBICTest::Schema::CD', 'cd_id');
__PACKAGE__->has_many('images', 'DBICTest::Schema::Image', 'artwork_id');

__PACKAGE__->has_many('artwork_to_artist', 'DBICTest::Schema::Artwork_to_Artist', 'artwork_cd_id');
__PACKAGE__->many_to_many('artists', 'artwork_to_artist', 'artist');

1;
