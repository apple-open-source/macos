package # hide from PAUSE 
    DBICTest::Schema::OneKey;

use base 'DBIx::Class::Core';

DBICTest::Schema::OneKey->table('onekey');
DBICTest::Schema::OneKey->add_columns(
  'id' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'artist' => {
    data_type => 'integer',
  },
  'cd' => {
    data_type => 'integer',
  },
);
DBICTest::Schema::OneKey->set_primary_key('id');


1;
