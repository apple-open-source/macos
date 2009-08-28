package MyDatabase::Main;
use base qw/DBIx::Class::Schema/;
__PACKAGE__->load_classes(qw/Artist Cd Track/);

1;
