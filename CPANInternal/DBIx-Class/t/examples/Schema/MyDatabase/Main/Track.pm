package MyDatabase::Main::Track;
use base qw/DBIx::Class/;
__PACKAGE__->load_components(qw/PK::Auto Core/);
__PACKAGE__->table('track');
__PACKAGE__->add_columns(qw/ trackid cd title/);
__PACKAGE__->set_primary_key('trackid');
__PACKAGE__->belongs_to('cd' => 'MyDatabase::Main::Cd');

1;
