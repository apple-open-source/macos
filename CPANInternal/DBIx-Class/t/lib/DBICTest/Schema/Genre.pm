package DBICTest::Schema::Genre;

use strict;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('genre');
__PACKAGE__->add_columns(
    genreid => {
      data_type => 'integer',
      is_auto_increment => 1,
    },
    name => {
      data_type => 'varchar',
      size => 100,
    },
);
__PACKAGE__->set_primary_key('genreid');
__PACKAGE__->add_unique_constraint ( genre_name => [qw/name/] );

__PACKAGE__->has_many (cds => 'DBICTest::Schema::CD', 'genreid');

__PACKAGE__->has_one (model_cd => 'DBICTest::Schema::CD', 'genreid');

1;
