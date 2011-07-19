package # hide from PAUSE 
    DBICTest::Taint::Classes::Auto;

use base 'DBIx::Class::Core';
__PACKAGE__->table('test');

1;
