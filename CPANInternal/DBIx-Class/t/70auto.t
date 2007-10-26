use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 2;

$schema->class("Artist")->load_components(qw/PK::Auto::SQLite/);
  # Should just be PK::Auto but this ensures the compat shim works

# add an artist without primary key to test Auto
my $artist = $schema->resultset("Artist")->create( { name => 'Auto' } );
$artist->name( 'Auto Change' );
ok($artist->update, 'update on object created without PK ok');

my $copied = $artist->copy({ name => 'Don\'t tell the RIAA', artistid => undef });
is($copied->name, 'Don\'t tell the RIAA', "Copied with PKs ok.");

