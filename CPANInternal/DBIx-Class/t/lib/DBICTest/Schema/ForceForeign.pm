package # hide from PAUSE
    DBICTest::Schema::ForceForeign;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('forceforeign');
__PACKAGE__->add_columns(
  'artist' => { data_type => 'integer' },
  'cd' => { data_type => 'integer' },
);
__PACKAGE__->set_primary_key(qw/artist/);

# Normally this would not appear as a FK constraint
# since it uses the PK
__PACKAGE__->might_have('artist_1', 'DBICTest::Schema::Artist',
  { 'foreign.artistid' => 'self.artist' },
  { is_foreign_key_constraint => 1 },
);

# Normally this would appear as a FK constraint
__PACKAGE__->might_have('cd_1', 'DBICTest::Schema::CD',
  { 'foreign.cdid' => 'self.cd' },
  { is_foreign_key_constraint => 0 },
);

# Normally this would appear as a FK constraint
__PACKAGE__->belongs_to('cd_3', 'DBICTest::Schema::CD',
  { 'foreign.cdid' => 'self.cd' },
  { is_foreign_key_constraint => 0 },
);

1;
