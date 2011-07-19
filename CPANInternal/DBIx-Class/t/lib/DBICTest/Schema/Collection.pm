package # hide from PAUSE 
    DBICTest::Schema::Collection;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('collection');
__PACKAGE__->add_columns(
  'collectionid' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'name' => {
    data_type => 'varchar',
    size      => 100,
  },
);
__PACKAGE__->set_primary_key('collectionid');

__PACKAGE__->has_many( collection_object => "DBICTest::Schema::CollectionObject",
                       { "foreign.collection" => "self.collectionid" }
                     );
__PACKAGE__->many_to_many( objects => collection_object => "object" );
__PACKAGE__->many_to_many( pointy_objects => collection_object => "object",
                           { where => { "object.type" => "pointy" } }
                         );
__PACKAGE__->many_to_many( round_objects => collection_object => "object",
                           { where => { "object.type" => "round" } } 
                         );

1;
