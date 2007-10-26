package # hide from PAUSE 
    DBICTest::Plain::Test;

use base 'DBIx::Class::Core';

__PACKAGE__->table('test');
__PACKAGE__->add_columns(
  'id' => {
    data_type => 'integer',
    is_auto_increment => 1
  },
  'name' => {
    data_type => 'varchar',
  },
);
__PACKAGE__->set_primary_key('id');

1;
