package # hide from PAUSE 
    DBICTest::Schema::TreeLike;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('treelike');
__PACKAGE__->add_columns(
  'id' => { data_type => 'integer', is_auto_increment => 1 },
  'parent' => { data_type => 'integer' , is_nullable=>1},
  'name' => { data_type => 'varchar',
    size      => 100,
 },
);
__PACKAGE__->set_primary_key(qw/id/);
__PACKAGE__->belongs_to('parent', 'TreeLike',
                          { 'foreign.id' => 'self.parent' });
__PACKAGE__->has_many('children', 'TreeLike', { 'foreign.parent' => 'self.id' });

## since this is a self referential table we need to do a post deploy hook and get
## some data in while constraints are off

 sub sqlt_deploy_hook {
   my ($self, $sqlt_table) = @_;

   ## We don't seem to need this anymore, but keeping it for the moment
   ## $sqlt_table->add_index(name => 'idx_name', fields => ['name']);
 }
1;
