package # hide from PAUSE 
    DBICTest::Schema::Year1999CDs;
## Used in 104view.t

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table_class('DBIx::Class::ResultSource::View');

__PACKAGE__->table('year1999cds');
__PACKAGE__->result_source_instance->is_virtual(1);
__PACKAGE__->result_source_instance->view_definition(
  "SELECT cdid, artist, title, single_track FROM cd WHERE year ='1999'"
);
__PACKAGE__->add_columns(
  'cdid' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'artist' => {
    data_type => 'integer',
  },
  'title' => {
    data_type => 'varchar',
    size      => 100,
  },
  'single_track' => {
    data_type => 'integer',
    is_nullable => 1,
    is_foreign_key => 1,
  },
);
__PACKAGE__->set_primary_key('cdid');
__PACKAGE__->add_unique_constraint([ qw/artist title/ ]);

__PACKAGE__->belongs_to( artist => 'DBICTest::Schema::Artist' );
__PACKAGE__->has_many( tracks => 'DBICTest::Schema::Track',
    { "foreign.cd" => "self.cdid" });

1;
