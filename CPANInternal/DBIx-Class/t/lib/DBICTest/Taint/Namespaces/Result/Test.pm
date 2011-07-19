package # hide from PAUSE 
    DBICTest::Taint::Namespaces::Result::Test;

use base 'DBIx::Class::Core';
__PACKAGE__->table('test');

1;
