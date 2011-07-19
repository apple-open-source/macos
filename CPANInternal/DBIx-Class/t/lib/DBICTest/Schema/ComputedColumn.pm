package # hide from PAUSE 
    DBICTest::Schema::ComputedColumn;

# for sybase and mssql computed column tests

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('computed_column_test');

__PACKAGE__->add_columns(
  'id' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'a_computed_column' => {
    data_type => undef,
    is_nullable => 0,
    default_value => \'getdate()',
  },
  'a_timestamp' => {
    data_type => 'timestamp',
    is_nullable => 0,
  },
  'charfield' => {
    data_type => 'varchar',
    size => 20,
    default_value => 'foo',
    is_nullable => 0,
  }
);

__PACKAGE__->set_primary_key('id');

1;
