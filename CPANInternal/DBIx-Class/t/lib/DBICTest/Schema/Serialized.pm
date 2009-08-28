package # hide from PAUSE 
    DBICTest::Schema::Serialized;

use base 'DBIx::Class::Core';

__PACKAGE__->table('serialized');
__PACKAGE__->add_columns(
  'id' => { data_type => 'integer' },
  'serialized' => { data_type => 'text' },
);
__PACKAGE__->set_primary_key('id');

1;
