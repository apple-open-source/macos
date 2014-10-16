use Graph::Directed ;

use Test::More tests => 2;

my $g0 = Graph::Directed->new() ;

$g0->add_weighted_edge('A', 'A1', 1) ;
$g0->add_weighted_edge('A', 'A2', 1) ;
$g0->add_weighted_edge('A1', 'A2', 1) ;
$g0->add_weighted_edge('A2', 'A1', 1) ;
$g0->add_weighted_edge('A1', 'L1', 100) ;
$g0->add_weighted_edge('A2', 'L2', 100) ;
$g0->add_weighted_edge('L1', 'B1', 100) ;
$g0->add_weighted_edge('L2', 'B2', 100) ;
$g0->add_weighted_edge('B1', 'B', 1) ;
$g0->add_weighted_edge('B2', 'B', 2) ;
$g0->add_weighted_edge('B1', 'B2', 1) ;
$g0->add_weighted_edge('B2', 'B1', 1) ;

my $SSSP0 = $g0->SPT_Dijkstra(first_root=>'A') ;

is($SSSP0, "A-A1,A-A2,A1-L1,A2-L2,B1-B,L1-B1,L2-B2");

my $g1 = Graph::Directed->new() ;

$g1->add_weighted_edge('A', 'A1', 1) ;
$g1->add_weighted_edge('A', 'A2', 1) ;
$g1->add_weighted_edge('A1', 'A2', 1) ;
$g1->add_weighted_edge('A2', 'A1', 1) ;
$g1->add_weighted_edge('A1', 'L1', 100) ;
$g1->add_weighted_edge('A2', 'L2', 100) ;
$g1->add_weighted_edge('L1', 'B3', 100) ;
$g1->add_weighted_edge('L2', 'B2', 100) ;
$g1->add_weighted_edge('B3', 'B', 1) ;
$g1->add_weighted_edge('B2', 'B', 2) ;
$g1->add_weighted_edge('B3', 'B2', 1) ;
$g1->add_weighted_edge('B2', 'B3', 1) ;

my $SSSP1 = $g1->SPT_Dijkstra(first_root=>'A') ;

is($SSSP1, "A-A1,A-A2,A1-L1,A2-L2,B3-B,L1-B3,L2-B2");


