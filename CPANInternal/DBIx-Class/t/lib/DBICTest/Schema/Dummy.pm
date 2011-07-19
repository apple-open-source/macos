package # hide from PAUSE
    DBICTest::Schema::Dummy;

use base qw/DBICTest::BaseResult/;

use strict;
use warnings;

__PACKAGE__->table('dummy');
__PACKAGE__->add_columns(
    'id' => {
        data_type => 'integer',
        is_auto_increment => 1
    },
    'gittery' => {
        data_type => 'varchar',
        size      => 100,
        is_nullable => 1,
    },
);
__PACKAGE__->set_primary_key('id');

1;
