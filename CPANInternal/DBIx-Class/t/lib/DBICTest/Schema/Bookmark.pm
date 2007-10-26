package # hide from PAUSE
    DBICTest::Schema::Bookmark;

    use base 'DBIx::Class::Core';


use strict;
use warnings;

__PACKAGE__->table('bookmark');
__PACKAGE__->add_columns(qw/id link/);
__PACKAGE__->add_columns(
    'id' => {
        data_type => 'integer',
        is_auto_increment => 1
    },
    'link' => {
        data_type => 'integer',
    },
);

__PACKAGE__->set_primary_key('id');
__PACKAGE__->belongs_to(link => 'DBICTest::Schema::Link' );

1;
