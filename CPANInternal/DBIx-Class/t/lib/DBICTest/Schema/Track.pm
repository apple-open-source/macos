package # hide from PAUSE 
    DBICTest::Schema::Track;

use base qw/DBICTest::BaseResult/;
__PACKAGE__->load_components(qw/InflateColumn::DateTime Ordered/);

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
    data_type => 'int',
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
  last_updated_at => {
    data_type => 'datetime',
    is_nullable => 1
  },
  small_dt => { # for mssql and sybase DT tests
    data_type => 'smalldatetime',
    is_nullable => 1
  },
);
__PACKAGE__->set_primary_key('trackid');

__PACKAGE__->add_unique_constraint([ qw/cd position/ ]);
__PACKAGE__->add_unique_constraint([ qw/cd title/ ]);

__PACKAGE__->position_column ('position');
__PACKAGE__->grouping_column ('cd');


__PACKAGE__->belongs_to( cd => 'DBICTest::Schema::CD' );
__PACKAGE__->belongs_to( disc => 'DBICTest::Schema::CD' => 'cd');

__PACKAGE__->might_have( cd_single => 'DBICTest::Schema::CD', 'single_track' );
__PACKAGE__->might_have( lyrics => 'DBICTest::Schema::Lyrics', 'track_id' );

__PACKAGE__->belongs_to(
    "year1999cd",
    "DBICTest::Schema::Year1999CDs",
    { "foreign.cdid" => "self.cd" },
    { join_type => 'left' },  # the relationship is of course optional
);
__PACKAGE__->belongs_to(
    "year2000cd",
    "DBICTest::Schema::Year2000CDs",
    { "foreign.cdid" => "self.cd" },
    { join_type => 'left' },
);

1;
