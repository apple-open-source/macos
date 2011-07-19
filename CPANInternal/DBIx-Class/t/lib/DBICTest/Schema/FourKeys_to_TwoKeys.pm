package # hide from PAUSE 
    DBICTest::Schema::FourKeys_to_TwoKeys;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('fourkeys_to_twokeys');
__PACKAGE__->add_columns(
  'f_foo' => { data_type => 'integer' },
  'f_bar' => { data_type => 'integer' },
  'f_hello' => { data_type => 'integer' },
  'f_goodbye' => { data_type => 'integer' },
  't_artist' => { data_type => 'integer' },
  't_cd' => { data_type => 'integer' },
  'autopilot' => { data_type => 'character' },
  'pilot_sequence' => { data_type => 'integer', is_nullable => 1 },
);
__PACKAGE__->set_primary_key(
  qw/f_foo f_bar f_hello f_goodbye t_artist t_cd/
);

__PACKAGE__->belongs_to('fourkeys', 'DBICTest::Schema::FourKeys', {
  'foreign.foo' => 'self.f_foo',
  'foreign.bar' => 'self.f_bar',
  'foreign.hello' => 'self.f_hello',
  'foreign.goodbye' => 'self.f_goodbye',
});

__PACKAGE__->belongs_to('twokeys', 'DBICTest::Schema::TwoKeys', {
  'foreign.artist' => 'self.t_artist',
  'foreign.cd' => 'self.t_cd',
});

1;
