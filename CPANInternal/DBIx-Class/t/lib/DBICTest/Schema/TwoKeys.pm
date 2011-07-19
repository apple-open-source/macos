package # hide from PAUSE
    DBICTest::Schema::TwoKeys;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('twokeys');
__PACKAGE__->add_columns(
  'artist' => { data_type => 'integer' },
  'cd' => { data_type => 'integer' },
);
__PACKAGE__->set_primary_key(qw/artist cd/);

__PACKAGE__->belongs_to(
    artist => 'DBICTest::Schema::Artist',
    {'foreign.artistid'=>'self.artist'},
);

__PACKAGE__->belongs_to( cd => 'DBICTest::Schema::CD', undef, { is_deferrable => 0, add_fk_index => 0 } );

__PACKAGE__->has_many(
  'fourkeys_to_twokeys', 'DBICTest::Schema::FourKeys_to_TwoKeys', {
    'foreign.t_artist' => 'self.artist',
    'foreign.t_cd' => 'self.cd',
});

__PACKAGE__->many_to_many(
  'fourkeys', 'fourkeys_to_twokeys', 'fourkeys',
);

1;
