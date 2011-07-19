package DBICTest::Schema::EventTZPg;

use strict;
use warnings;
use base qw/DBICTest::BaseResult/;

__PACKAGE__->load_components(qw/InflateColumn::DateTime/);

__PACKAGE__->table('event');

__PACKAGE__->add_columns(
  id => { data_type => 'integer', is_auto_increment => 1 },
  starts_at => { data_type => 'datetime', timezone => "America/Chicago", locale => 'de_DE' },
  created_on => { data_type => 'timestamp with time zone', timezone => "America/Chicago" },
  ts_without_tz => { data_type => 'timestamp without time zone' },
);

__PACKAGE__->set_primary_key('id');

sub _datetime_parser {
  require DateTime::Format::Pg;
  DateTime::Format::Pg->new();
}

1;
