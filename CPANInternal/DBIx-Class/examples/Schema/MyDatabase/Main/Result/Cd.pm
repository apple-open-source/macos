package MyDatabase::Main::Result::Cd;

use warnings;
use strict;

use base qw/DBIx::Class::Core/;

__PACKAGE__->table('cd');

__PACKAGE__->add_columns(qw/ cdid artist title/);

__PACKAGE__->set_primary_key('cdid');

__PACKAGE__->belongs_to('artist' => 'MyDatabase::Main::Result::Artist');
__PACKAGE__->has_many('tracks' => 'MyDatabase::Main::Result::Track');

1;
