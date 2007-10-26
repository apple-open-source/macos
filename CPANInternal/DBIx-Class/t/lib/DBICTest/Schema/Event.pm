package DBICTest::Schema::Event;

use strict;
use warnings;
use base qw/DBIx::Class::Core/;

__PACKAGE__->load_components(qw/InflateColumn::DateTime/);

__PACKAGE__->table('event');

__PACKAGE__->add_columns(
  id => { data_type => 'integer', is_auto_increment => 1 },
  starts_at => { data_type => 'datetime' },
  created_on => { data_type => 'timestamp' }
);

__PACKAGE__->set_primary_key('id');

1;
