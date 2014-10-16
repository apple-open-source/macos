use Test::More tests => 2;

use Graph;

my $g1 = Graph->new;

ok ( !$g1->hypervertexed );

my $g2 = Graph->new( hypervertexed => 1 );

ok (  $g2->hypervertexed );

