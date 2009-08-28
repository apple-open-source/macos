package DBICNSTest::OtherRslt::D;
use base qw/DBIx::Class/;
__PACKAGE__->load_components(qw/PK::Auto Core/);
__PACKAGE__->table('d');
__PACKAGE__->add_columns('d');
1;
