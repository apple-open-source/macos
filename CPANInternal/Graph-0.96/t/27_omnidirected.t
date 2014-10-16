use Test::More tests => 2;

use Graph;

my $g1 = Graph->new;

ok ( !$g1->omnidirected );

my $g2 = Graph->new( omnidirected => 1 );

ok (  $g2->omnidirected );

