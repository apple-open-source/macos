package DBICNSTest::Result::A;
use base qw/DBIx::Class/;
__PACKAGE__->load_components(qw/PK::Auto Core/);
__PACKAGE__->table('a');
__PACKAGE__->add_columns('a');
1;
