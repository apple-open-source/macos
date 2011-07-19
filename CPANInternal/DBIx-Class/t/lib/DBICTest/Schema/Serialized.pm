package # hide from PAUSE 
    DBICTest::Schema::Serialized;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('serialized');
__PACKAGE__->add_columns(
  'id' => { data_type => 'integer', is_auto_increment => 1 },
  'serialized' => { data_type => 'text' },
);
__PACKAGE__->set_primary_key('id');

1;
