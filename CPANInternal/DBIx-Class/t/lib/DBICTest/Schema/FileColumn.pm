package 
DBICTest::Schema::FileColumn;

use strict;
use warnings;
use base qw/DBICTest::BaseResult/;
use File::Temp qw/tempdir/;

__PACKAGE__->load_components(qw/InflateColumn::File/);

__PACKAGE__->table('file_columns');

__PACKAGE__->add_columns(
  id => { data_type => 'integer', is_auto_increment => 1 },
  file => {
    data_type        => 'varchar',
    is_file_column   => 1,
    file_column_path => tempdir(CLEANUP => 1),
    size             => 255
  }
);

__PACKAGE__->set_primary_key('id');

1;
