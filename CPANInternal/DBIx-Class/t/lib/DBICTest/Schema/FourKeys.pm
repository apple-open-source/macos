package # hide from PAUSE 
    DBICTest::Schema::FourKeys;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('fourkeys');
__PACKAGE__->add_columns(
  'foo' => { data_type => 'integer' },
  'bar' => { data_type => 'integer' },
  'hello' => { data_type => 'integer' },
  'goodbye' => { data_type => 'integer' },
  'sensors' => { data_type => 'character', size => 10 },
  'read_count' => { data_type => 'integer', is_nullable => 1 },
);
__PACKAGE__->set_primary_key(qw/foo bar hello goodbye/);

__PACKAGE__->has_many(
  'fourkeys_to_twokeys', 'DBICTest::Schema::FourKeys_to_TwoKeys', {
    'foreign.f_foo' => 'self.foo',
    'foreign.f_bar' => 'self.bar',
    'foreign.f_hello' => 'self.hello',
    'foreign.f_goodbye' => 'self.goodbye',
});

__PACKAGE__->many_to_many(
  'twokeys', 'fourkeys_to_twokeys', 'twokeys',
);

1;
