package # hide from PAUSE 
    DBICTest::Schema::Serialized;

use base 'DBIx::Class::Core';

DBICTest::Schema::Serialized->table('serialized');
DBICTest::Schema::Serialized->add_columns(
  'id' => { data_type => 'integer' },
  'serialized' => { data_type => 'text' },
);
DBICTest::Schema::Serialized->set_primary_key('id');

1;
