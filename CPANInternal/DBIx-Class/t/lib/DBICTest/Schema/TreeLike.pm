package # hide from PAUSE 
    DBICTest::Schema::TreeLike;

use base qw/DBIx::Class::Core/;

__PACKAGE__->table('treelike');
__PACKAGE__->add_columns(
  'id' => { data_type => 'integer', is_auto_increment => 1 },
  'parent' => { data_type => 'integer' },
  'name' => { data_type => 'varchar',
    size      => 100,
 },
);
__PACKAGE__->set_primary_key(qw/id/);
__PACKAGE__->belongs_to('parent', 'TreeLike',
                          { 'foreign.id' => 'self.parent' });
__PACKAGE__->has_many('children', 'TreeLike', { 'foreign.parent' => 'self.id' });

1;
