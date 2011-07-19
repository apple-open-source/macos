package # hide from PAUSE 
    DBICTest::Schema::BooksInLibrary;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('books');
__PACKAGE__->add_columns(
  'id' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'source' => {
    data_type => 'varchar',
    size      => '100',
  },
  'owner' => {
    data_type => 'integer',
  },
  'title' => {
    data_type => 'varchar',
    size      => '100',
  },
  'price' => {
    data_type => 'integer',
    is_nullable => 1,
  },
);
__PACKAGE__->set_primary_key('id');

__PACKAGE__->resultset_attributes({where => { source => "Library" } });

__PACKAGE__->belongs_to ( owner => 'DBICTest::Schema::Owners', 'owner' );

1;
