use Test::More tests => 2;

use Graph;

my $g = Graph->new;

isa_ok($g, 'Graph');

my $h = $g->new;

isa_ok($g, 'Graph');
