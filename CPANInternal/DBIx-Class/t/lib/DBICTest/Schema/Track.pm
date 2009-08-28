package # hide from PAUSE 
    DBICTest::Schema::Track;

use base 'DBIx::Class::Core';
__PACKAGE__->load_components(qw/InflateColumn::DateTime/);

__PACKAGE__->table('track');
__PACKAGE__->add_columns(
  'trackid' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'cd' => {
    data_type => 'integer',
  },
  'position' => {
    data_type => 'integer',
    accessor => 'pos',
  },
  'title' => {
    data_type => 'varchar',
    size      => 100,
  },
  last_updated_on => {
    data_type => 'datetime',
    accessor => 'updated_date',
    is_nullable => 1
  },
);
__PACKAGE__->set_primary_key('trackid');

__PACKAGE__->add_unique_constraint([ qw/cd position/ ]);
__PACKAGE__->add_unique_constraint([ qw/cd title/ ]);

__PACKAGE__->belongs_to( cd => 'DBICTest::Schema::CD' );
__PACKAGE__->belongs_to( disc => 'DBICTest::Schema::CD' => 'cd');

1;
