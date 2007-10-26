use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

plan tests => 1;

my $schema = DBICTest->init_schema();

is( ref($schema->storage), 'DBIx::Class::Storage::DBI::SQLite',
    'Storage reblessed correctly into DBIx::Class::Storage::DBI::SQLite' );

1;
