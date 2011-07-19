package # hide from PAUSE 
    DBICTest::Schema::Tag;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('tags');
__PACKAGE__->add_columns(
  'tagid' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'cd' => {
    data_type => 'integer',
  },
  'tag' => {
    data_type => 'varchar',
    size      => 100,
  },
);
__PACKAGE__->set_primary_key('tagid');

__PACKAGE__->belongs_to( cd => 'DBICTest::Schema::CD' );

1;
