package # hide from PAUSE 
    DBICTest::ResultSetManager::Foo;
use base 'DBIx::Class';

__PACKAGE__->load_components(qw/ ResultSetManager Core /);
__PACKAGE__->table('foo');

sub bar : ResultSet { 'good' }

1;
