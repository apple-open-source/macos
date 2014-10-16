use Test::More tests => 4;

use lib 't';
use MyGraph;

my $g = MyGraph->new;

isa_ok($g, 'MyGraph');
isa_ok($g, 'Graph');

my $h = $g->new;

isa_ok($h, 'MyGraph');
isa_ok($h, 'Graph');

