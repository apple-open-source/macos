package # hide from PAUSE 
    DBICTest::Schema::Lyrics;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('lyrics');
__PACKAGE__->add_columns(
  'lyric_id' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'track_id' => {
    data_type => 'integer',
    is_foreign_key => 1,
  },
);
__PACKAGE__->set_primary_key('lyric_id');
__PACKAGE__->belongs_to('track', 'DBICTest::Schema::Track', 'track_id');
__PACKAGE__->has_many('lyric_versions', 'DBICTest::Schema::LyricVersion', 'lyric_id');

1;
