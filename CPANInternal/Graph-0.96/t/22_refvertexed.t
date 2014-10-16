use Test::More tests => 2;

use Graph;

my $g1 = Graph->new;

ok ( !$g1->refvertexed );

my $g2 = Graph->new( refvertexed => 1 );

ok (  $g2->refvertexed );

