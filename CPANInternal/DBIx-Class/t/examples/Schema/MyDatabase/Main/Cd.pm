package MyDatabase::Main::Cd;
use base qw/DBIx::Class/;
__PACKAGE__->load_components(qw/PK::Auto Core/);
__PACKAGE__->table('cd');
__PACKAGE__->add_columns(qw/ cdid artist title/);
__PACKAGE__->set_primary_key('cdid');
__PACKAGE__->belongs_to('artist' => 'MyDatabase::Main::Artist');
__PACKAGE__->has_many('tracks' => 'MyDatabase::Main::Track');

1;
