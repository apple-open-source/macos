package # hide from PAUSE 
    DBICTest::Schema::BindType;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('bindtype_test');

__PACKAGE__->add_columns(
  'id' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'bytea' => {
    data_type => 'bytea',
    is_nullable => 1,
  },
  'blob' => {
    data_type => 'blob',
    is_nullable => 1,
  },
  'clob' => {
    data_type => 'clob',
    is_nullable => 1,
  },
);

__PACKAGE__->set_primary_key('id');

1;
