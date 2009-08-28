package # hide from PAUSE 
    DBICTest::Schema::NoPrimaryKey;

use base 'DBIx::Class::Core';

__PACKAGE__->table('noprimarykey');
__PACKAGE__->add_columns(
  'foo' => { data_type => 'integer' },
  'bar' => { data_type => 'integer' },
  'baz' => { data_type => 'integer' },
);

__PACKAGE__->add_unique_constraint(foo_bar => [ qw/foo bar/ ]);

1;
