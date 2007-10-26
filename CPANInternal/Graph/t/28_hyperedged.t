use Test::More tests => 2;

use Graph;

my $g1 = Graph->new;

ok ( !$g1->hyperedged );

my $g2 = Graph->new( hyperedged => 1 );

ok (  $g2->hyperedged );

