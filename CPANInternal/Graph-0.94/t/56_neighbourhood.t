use Test::More tests => 92;

use Graph;
my $g0 = Graph->new;
my $g1 = Graph->new(undirected => 1);

$g0->add_edge(1=>1); $g1->add_edge(1=>1);
$g0->add_edge(1=>2); $g1->add_edge(1=>2);
$g0->add_edge(1=>3); $g1->add_edge(1=>3);
$g0->add_edge(2=>4); $g1->add_edge(2=>4);
$g0->add_edge(5=>4); $g1->add_edge(5=>4);
$g0->add_vertex(6);  $g1->add_vertex(6);  

is( "@{[sort $g0->successors(1)]}", "1 2 3" );
is( "@{[sort $g0->successors(2)]}", "4" );
is( "@{[sort $g0->successors(3)]}", "" );
is( "@{[sort $g0->successors(4)]}", "" );
is( "@{[sort $g0->successors(5)]}", "4" );
is( "@{[sort $g0->successors(6)]}", "" );

is( "@{[sort $g0->predecessors(1)]}", "1" );
is( "@{[sort $g0->predecessors(2)]}", "1" );
is( "@{[sort $g0->predecessors(3)]}", "1" );
is( "@{[sort $g0->predecessors(4)]}", "2 5" );
is( "@{[sort $g0->predecessors(5)]}", "" );
is( "@{[sort $g0->predecessors(6)]}", "" );

is( "@{[sort $g0->neighbours(1)]}", "1 2 3" );
is( "@{[sort $g0->neighbours(2)]}", "1 4" );
is( "@{[sort $g0->neighbours(3)]}", "1" );
is( "@{[sort $g0->neighbours(4)]}", "2 5" );
is( "@{[sort $g0->neighbours(5)]}", "4" );
is( "@{[sort $g0->neighbours(6)]}", "" );

is( "@{[sort $g1->successors(1)]}", "1 2 3" );
is( "@{[sort $g1->successors(2)]}", "1 4" );
is( "@{[sort $g1->successors(3)]}", "1" );
is( "@{[sort $g1->successors(4)]}", "2 5" );
is( "@{[sort $g1->successors(5)]}", "4" );
is( "@{[sort $g1->successors(6)]}", "" );

is( "@{[sort $g1->predecessors(1)]}", "1 2 3" );
is( "@{[sort $g1->predecessors(2)]}", "1 4" );
is( "@{[sort $g1->predecessors(3)]}", "1" );
is( "@{[sort $g1->predecessors(4)]}", "2 5" );
is( "@{[sort $g1->predecessors(5)]}", "4" );
is( "@{[sort $g1->predecessors(6)]}", "" );

is( "@{[sort $g1->neighbours(1)]}", "1 2 3" );
is( "@{[sort $g1->neighbours(2)]}", "1 4" );
is( "@{[sort $g1->neighbours(3)]}", "1" );
is( "@{[sort $g1->neighbours(4)]}", "2 5" );
is( "@{[sort $g1->neighbours(5)]}", "4" );
is( "@{[sort $g1->neighbours(6)]}", "" );

ok(!$g0->is_successorless_vertex(1));
ok(!$g0->is_successorless_vertex(2));
ok( $g0->is_successorless_vertex(3));
ok( $g0->is_successorless_vertex(4));
ok(!$g0->is_successorless_vertex(5));
ok( $g0->is_successorless_vertex(6));

ok(!$g0->is_predecessorless_vertex(1));
ok(!$g0->is_predecessorless_vertex(2));
ok(!$g0->is_predecessorless_vertex(3));
ok(!$g0->is_predecessorless_vertex(4));
ok( $g0->is_predecessorless_vertex(5));
ok( $g0->is_predecessorless_vertex(6));

ok( $g0->is_successorful_vertex(1));
ok( $g0->is_successorful_vertex(2));
ok(!$g0->is_successorful_vertex(3));
ok(!$g0->is_successorful_vertex(4));
ok( $g0->is_successorful_vertex(5));
ok(!$g0->is_successorful_vertex(6));

ok( $g0->is_predecessorful_vertex(1));
ok( $g0->is_predecessorful_vertex(2));
ok( $g0->is_predecessorful_vertex(3));
ok( $g0->is_predecessorful_vertex(4));
ok(!$g0->is_predecessorful_vertex(5));
ok(!$g0->is_predecessorful_vertex(6));

is("@{[sort $g0->successorless_vertices]}",   "3 4 6");
is("@{[sort $g0->predecessorless_vertices]}", "5 6");

is("@{[sort $g0->successorful_vertices]}",   "1 2 5");
is("@{[sort $g0->predecessorful_vertices]}", "1 2 3 4");

ok(!$g1->is_successorless_vertex(1));
ok(!$g1->is_successorless_vertex(2));
ok(!$g1->is_successorless_vertex(3));
ok(!$g1->is_successorless_vertex(4));
ok(!$g1->is_successorless_vertex(5));
ok( $g1->is_successorless_vertex(6));

ok(!$g1->is_predecessorless_vertex(1));
ok(!$g1->is_predecessorless_vertex(2));
ok(!$g1->is_predecessorless_vertex(3));
ok(!$g1->is_predecessorless_vertex(4));
ok(!$g1->is_predecessorless_vertex(5));
ok( $g1->is_predecessorless_vertex(6));

ok( $g1->is_successorful_vertex(1));
ok( $g1->is_successorful_vertex(2));
ok( $g1->is_successorful_vertex(3));
ok( $g1->is_successorful_vertex(4));
ok( $g1->is_successorful_vertex(5));
ok(!$g1->is_successorful_vertex(6));

ok( $g1->is_predecessorful_vertex(1));
ok( $g1->is_predecessorful_vertex(2));
ok( $g1->is_predecessorful_vertex(3));
ok( $g1->is_predecessorful_vertex(4));
ok( $g1->is_predecessorful_vertex(5));
ok(!$g1->is_predecessorful_vertex(6));

is("@{[sort $g1->successorless_vertices]}",   "6");
is("@{[sort $g1->predecessorless_vertices]}", "6");

is("@{[sort $g1->successorful_vertices]}",   "1 2 3 4 5");
is("@{[sort $g1->predecessorful_vertices]}", "1 2 3 4 5");

