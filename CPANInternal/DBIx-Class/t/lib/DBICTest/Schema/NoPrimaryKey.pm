package # hide from PAUSE 
    DBICTest::Schema::NoPrimaryKey;

use base 'DBIx::Class::Core';

DBICTest::Schema::NoPrimaryKey->table('noprimarykey');
DBICTest::Schema::NoPrimaryKey->add_columns(
  'foo' => { data_type => 'integer' },
  'bar' => { data_type => 'integer' },
  'baz' => { data_type => 'integer' },
);

DBICTest::Schema::NoPrimaryKey->add_unique_constraint(foo_bar => [ qw/foo bar/ ]);

1;
