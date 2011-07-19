package # hide from PAUSE 
    DBICTest::Schema::Image;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('images');
__PACKAGE__->add_columns(
  'id' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'artwork_id' => {
    data_type => 'integer',
    is_foreign_key => 1,
  },
  'name' => {
    data_type => 'varchar',
    size => 100,
  },
  'data' => {
    data_type => 'blob',
    is_nullable => 1,
  },
);
__PACKAGE__->set_primary_key('id');
__PACKAGE__->belongs_to('artwork', 'DBICTest::Schema::Artwork', 'artwork_id');

1;
