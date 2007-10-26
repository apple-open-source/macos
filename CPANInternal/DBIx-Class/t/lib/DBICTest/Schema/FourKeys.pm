package # hide from PAUSE 
    DBICTest::Schema::FourKeys;

use base 'DBIx::Class::Core';

DBICTest::Schema::FourKeys->table('fourkeys');
DBICTest::Schema::FourKeys->add_columns(
  'foo' => { data_type => 'integer' },
  'bar' => { data_type => 'integer' },
  'hello' => { data_type => 'integer' },
  'goodbye' => { data_type => 'integer' },
  'sensors' => { data_type => 'character' },
);
DBICTest::Schema::FourKeys->set_primary_key(qw/foo bar hello goodbye/);

DBICTest::Schema::FourKeys->has_many(
  'fourkeys_to_twokeys', 'DBICTest::Schema::FourKeys_to_TwoKeys', {
    'foreign.f_foo' => 'self.foo',
    'foreign.f_bar' => 'self.bar',
    'foreign.f_hello' => 'self.hello',
    'foreign.f_goodbye' => 'self.goodbye',
});

DBICTest::Schema::FourKeys->many_to_many(
  'twokeys', 'fourkeys_to_twokeys', 'twokeys',
);

1;
