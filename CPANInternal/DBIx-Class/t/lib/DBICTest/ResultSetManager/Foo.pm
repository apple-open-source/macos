package # hide from PAUSE 
    DBICTest::ResultSetManager::Foo;
use base 'DBIx::Class::Core';

__PACKAGE__->load_components(qw/ ResultSetManager /);
__PACKAGE__->table('foo');

sub bar : ResultSet { 'good' }

1;
