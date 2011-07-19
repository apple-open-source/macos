package # hide from PAUSE 
    DBICTest::Schema::ArtistGUID;

use base qw/DBICTest::BaseResult/;

# test MSSQL uniqueidentifier type

__PACKAGE__->table('artist');
__PACKAGE__->add_columns(
  'artistid' => {
    data_type => 'uniqueidentifier' # auto_nextval not necessary for PK
  },
  'name' => {
    data_type => 'varchar',
    size      => 100,
    is_nullable => 1,
  },
  rank => {
    data_type => 'integer',
    default_value => 13,
  },
  charfield => {
    data_type => 'char',
    size => 10,
    is_nullable => 1,
  },
  a_guid => {
    data_type => 'uniqueidentifier',
    auto_nextval => 1, # necessary here, because not a PK
    is_nullable => 1,
  }
);
__PACKAGE__->set_primary_key('artistid');

1;
