package # hide from PAUSE 
    DBICTest::Schema::SelfRefAlias;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('self_ref_alias');
__PACKAGE__->add_columns(
  'self_ref' => {
    data_type => 'integer',
  },
  'alias' => {
    data_type => 'integer',
  },
);
__PACKAGE__->set_primary_key(qw/self_ref alias/);

__PACKAGE__->belongs_to( self_ref => 'DBICTest::Schema::SelfRef' );
__PACKAGE__->belongs_to( alias => 'DBICTest::Schema::SelfRef' );

1;
