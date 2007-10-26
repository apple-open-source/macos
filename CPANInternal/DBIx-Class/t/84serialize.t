use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use Storable;

my $schema = DBICTest->init_schema();

plan tests => 1;

my $artist = $schema->resultset('Artist')->find(1);
my $copy = eval { Storable::dclone($artist) };
is_deeply($copy, $artist, 'serialize row object works');

