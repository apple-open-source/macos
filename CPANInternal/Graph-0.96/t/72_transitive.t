use Test::More tests => 227;

use Graph::Directed;
use Graph::Undirected;

my $g0 = Graph::Directed->new;

$g0->add_edge(qw(a b));
$g0->add_edge(qw(a c));
$g0->add_edge(qw(c d));

ok(!$g0->is_transitive);

my $t0 = Graph::TransitiveClosure->new($g0);

ok( $t0->has_edge(qw(a a)));
ok( $t0->has_edge(qw(a b)));
ok( $t0->has_edge(qw(a c)));
ok(!$t0->has_edge(qw(d c)));
ok(!$t0->has_edge(qw(b a)));
ok( $t0->has_edge(qw(b b)));
ok(!$t0->has_edge(qw(b c)));
ok(!$t0->has_edge(qw(b d)));
ok(!$t0->has_edge(qw(c a)));
ok(!$t0->has_edge(qw(c b)));
ok( $t0->has_edge(qw(c c)));
ok( $t0->has_edge(qw(c d)));
ok(!$t0->has_edge(qw(d a)));
ok(!$t0->has_edge(qw(d b)));
ok(!$t0->has_edge(qw(d c)));
ok( $t0->has_edge(qw(d d)));

ok( $t0->is_transitive);

my $r0 = Graph::TransitiveClosure->new($g0, reflexive => 0);

ok(!$r0->has_edge(qw(a a)));
ok( $r0->has_edge(qw(a b)));
ok( $r0->has_edge(qw(a c)));
ok(!$r0->has_edge(qw(d c)));
ok(!$r0->has_edge(qw(b a)));
ok(!$r0->has_edge(qw(b b)));
ok(!$r0->has_edge(qw(b c)));
ok(!$r0->has_edge(qw(b d)));
ok(!$r0->has_edge(qw(c a)));
ok(!$r0->has_edge(qw(c b)));
ok(!$r0->has_edge(qw(c c)));
ok( $r0->has_edge(qw(c d)));
ok(!$r0->has_edge(qw(d a)));
ok(!$r0->has_edge(qw(d b)));
ok(!$r0->has_edge(qw(d c)));
ok(!$r0->has_edge(qw(d d)));

ok( $r0->is_transitive);

my $r1 = Graph::TransitiveClosure->new($g0, reflexive => 1);

ok( $r1->has_edge(qw(a a)));
ok( $r1->has_edge(qw(a b)));
ok( $r1->has_edge(qw(a c)));
ok(!$r1->has_edge(qw(d c)));
ok(!$r1->has_edge(qw(b a)));
ok( $r1->has_edge(qw(b b)));
ok(!$r1->has_edge(qw(b c)));
ok(!$r1->has_edge(qw(b d)));
ok(!$r1->has_edge(qw(c a)));
ok(!$r1->has_edge(qw(c b)));
ok( $r1->has_edge(qw(c c)));
ok( $r1->has_edge(qw(c d)));
ok(!$r1->has_edge(qw(d a)));
ok(!$r1->has_edge(qw(d b)));
ok(!$r1->has_edge(qw(d c)));
ok( $r1->has_edge(qw(d d)));

ok( $r1->is_transitive);

my $g1 = Graph::Undirected->new;

$g1->add_edge(qw(a b));
$g1->add_edge(qw(a c));
$g1->add_edge(qw(c d));

ok(!$g1->is_transitive);

my $t1 = Graph::TransitiveClosure->new($g1);

ok( $t1->has_edge(qw(a a)));
ok( $t1->has_edge(qw(a b)));
ok( $t1->has_edge(qw(a c)));
ok( $t1->has_edge(qw(d c)));
ok( $t1->has_edge(qw(b a)));
ok( $t1->has_edge(qw(b b)));
ok( $t1->has_edge(qw(b c)));
ok( $t1->has_edge(qw(b d)));
ok( $t1->has_edge(qw(c a)));
ok( $t1->has_edge(qw(c b)));
ok( $t1->has_edge(qw(c c)));
ok( $t1->has_edge(qw(c d)));
ok( $t1->has_edge(qw(d a)));
ok( $t1->has_edge(qw(d b)));
ok( $t1->has_edge(qw(d c)));
ok( $t1->has_edge(qw(d d)));

ok( $t1->is_transitive);

my $g2 = Graph->new;
$g2->add_weighted_edge(qw(a b 3));
$g2->add_weighted_edge(qw(b c 1));

ok(!$g2->is_transitive);

my $t2 = Graph::TransitiveClosure->new($g2, path => 1);

is($t2->path_length(qw(a a)), 0);
is($t2->path_length(qw(a b)), 3);
is($t2->path_length(qw(a c)), 4);
is($t2->path_length(qw(b a)), undef);
is($t2->path_length(qw(b b)), 0);
is($t2->path_length(qw(b c)), 1);
is($t2->path_length(qw(c a)), undef);
is($t2->path_length(qw(c b)), undef);
is($t2->path_length(qw(c c)), 0);

is("@{[$t2->path_vertices(qw(a a))]}", "");
is("@{[$t2->path_vertices(qw(a b))]}", "a b");
is("@{[$t2->path_vertices(qw(a c))]}", "a b c");
is("@{[$t2->path_vertices(qw(b a))]}", "");
is("@{[$t2->path_vertices(qw(b b))]}", "");
is("@{[$t2->path_vertices(qw(b c))]}", "b c");
is("@{[$t2->path_vertices(qw(c a))]}", "");
is("@{[$t2->path_vertices(qw(c b))]}", "");
is("@{[$t2->path_vertices(qw(c c))]}", "");

ok( $t2->is_transitive);

my $g3 = Graph->new;
$g3->add_edge(qw(a b));
$g3->add_edge(qw(b c));

ok(!$g3->is_transitive);

my $t3 = Graph::TransitiveClosure->new($g3, path => 1);

is($t3->path_length(qw(a a)), 0);
is($t3->path_length(qw(a b)), 1);
is($t3->path_length(qw(a c)), 2);
is($t3->path_length(qw(b a)), undef);
is($t3->path_length(qw(b b)), 0);
is($t3->path_length(qw(b c)), 1);
is($t3->path_length(qw(c a)), undef);
is($t3->path_length(qw(c b)), undef);
is($t3->path_length(qw(c c)), 0);

is("@{[$t3->path_vertices(qw(a a))]}", "");
is("@{[$t3->path_vertices(qw(a b))]}", "a b");
is("@{[$t3->path_vertices(qw(a c))]}", "a b c");
is("@{[$t3->path_vertices(qw(b a))]}", "");
is("@{[$t3->path_vertices(qw(b b))]}", "");
is("@{[$t3->path_vertices(qw(b c))]}", "b c");
is("@{[$t3->path_vertices(qw(c a))]}", "");
is("@{[$t3->path_vertices(qw(c b))]}", "");
is("@{[$t3->path_vertices(qw(c c))]}", "");

is($t3->path_predecessor(qw(a a)), undef);
is($t3->path_predecessor(qw(a b)), "b");
is($t3->path_predecessor(qw(a c)), "b");
is($t3->path_predecessor(qw(b a)), undef);
is($t3->path_predecessor(qw(b b)), undef);
is($t3->path_predecessor(qw(b c)), "c");
is($t3->path_predecessor(qw(c a)), undef);
is($t3->path_predecessor(qw(c b)), undef);
is($t3->path_predecessor(qw(c c)), undef);

ok( $t3->is_transitive);

is($g3->path_length(qw(a a)), 0);
is($g3->path_length(qw(a b)), 1);
is($g3->path_length(qw(a c)), 2);
is($g3->path_length(qw(b a)), undef);
is($g3->path_length(qw(b b)), 0);
is($g3->path_length(qw(b c)), 1);
is($g3->path_length(qw(c a)), undef);
is($g3->path_length(qw(c b)), undef);
is($g3->path_length(qw(c c)), 0);

is("@{[$g3->path_vertices(qw(a a))]}", "");
is("@{[$g3->path_vertices(qw(a b))]}", "a b");
is("@{[$g3->path_vertices(qw(a c))]}", "a b c");
is("@{[$g3->path_vertices(qw(b a))]}", "");
is("@{[$g3->path_vertices(qw(b b))]}", "");
is("@{[$g3->path_vertices(qw(b c))]}", "b c");
is("@{[$g3->path_vertices(qw(c a))]}", "");
is("@{[$g3->path_vertices(qw(c b))]}", "");
is("@{[$g3->path_vertices(qw(c c))]}", "");

is($g3->path_predecessor(qw(a a)), undef);
is($g3->path_predecessor(qw(a b)), "b");
is($g3->path_predecessor(qw(a c)), "b");
is($g3->path_predecessor(qw(b a)), undef);
is($g3->path_predecessor(qw(b b)), undef);
is($g3->path_predecessor(qw(b c)), "c");
is($g3->path_predecessor(qw(c a)), undef);
is($g3->path_predecessor(qw(c b)), undef);
is($g3->path_predecessor(qw(c c)), undef);

{
    # Found by Nathan Goodman.
    is($t3->path_vertices("a", "b"), 2);
    is($t3->path_vertices("a", "b"), 2); # Crashed or hung, depending.
}

{
    my $g4 = Graph::Directed->new;
    $g4->set_edge_attribute("a", "b", "distance", 2);
    $g4->set_edge_attribute("b", "c", "distance", 3);
    my $t4 = Graph::TransitiveClosure->new($g4,
					   attribute_name => 'distance',
					   path_length => 1);
    is($t4->path_length("a", "c"), 5);
}

{
    # Found by Nathan Goodman.
    use Graph::Directed;
    my $graph = new Graph::Directed;

    $graph->add_weighted_edge('a', 'b', 1);
    $graph->add_weighted_edge('b', 'a', 1);

    my $tc = new Graph::TransitiveClosure($graph,
					  path_length => 1,
					  path_vertices => 1);

    is($tc->path_length('a','a'),   0);
    is($tc->path_vertices('a','a'), 0);

    is($tc->path_length('b','b'),   0);
    is($tc->path_vertices('b','b'), 0);

    # Some extra ones.

    is($tc->path_length('a','b'),   1);
    is($tc->path_vertices('a','b'), 2);

    is($tc->path_length('b','a'),   1);
    is($tc->path_vertices('b','a'), 2);

    ok($tc->is_reachable('a', 'a'));
    ok($tc->is_reachable('a', 'b'));
    ok($tc->is_reachable('b', 'a'));
    ok($tc->is_reachable('b', 'b'));
}

{
    use Graph::Directed;
    my $graph = new Graph::Directed;

    $graph->add_edge('a', 'b');
    $graph->add_edge('b', 'a');

    my $tc = new Graph::TransitiveClosure($graph,
					  path_length => 1,
					  path_vertices => 1);

    is($tc->path_length('a','a'),   0);
    is($tc->path_vertices('a','a'), 0);

    is($tc->path_length('b','b'),   0);
    is($tc->path_vertices('b','b'), 0);

    is($tc->path_length('a','b'),   1);
    is($tc->path_vertices('a','b'), 2);

    is($tc->path_length('b', 'a'),  1);
    is($tc->path_vertices('b','a'), 2);

    ok($tc->is_reachable('a', 'a'));
    ok($tc->is_reachable('a', 'b'));
    ok($tc->is_reachable('b', 'a'));
    ok($tc->is_reachable('b', 'b'));
}

{
    # More Nathan Goodman.
    use Graph::Directed;
    my $graph = new Graph::Directed;

    $graph->add_weighted_edge('a', 'a', 1);
    my $tc = new Graph::TransitiveClosure($graph,
					  path_length => 1,
					  path_vertices => 1);

    ok($tc->is_reachable('a', 'a'));
    is($tc->path_length('a', 'a'), 0);
    is($tc->path_vertices('a', 'a'), 0);

    # More extra.
    is($tc->path_length('b','b'),   undef);
    is($tc->path_vertices('b','b'), undef);

    is($tc->path_length('a','b'),   undef);
    is($tc->path_vertices('a','b'), undef);

    is($tc->path_length('b', 'a'),  undef);
    is($tc->path_vertices('b','a'), undef);

    is($tc->is_reachable('a', 'b'), undef);
    is($tc->is_reachable('b', 'a'), undef);
    is($tc->is_reachable('b', 'b'), undef);
}

# TransitiveClosure_Floyd_Warshall is just an alias for TransitiveClosure.

my $t0tcfw = Graph->TransitiveClosure_Floyd_Warshall($g0);

is($t0, $t0tcfw);

my $t3apspfw = Graph::APSP_Floyd_Warshall($g3);

is($t3, $t3apspfw);

is($t3apspfw->path_length(qw(a a)), 0);
is($t3apspfw->path_length(qw(a b)), 1);
is($t3apspfw->path_length(qw(a c)), 2);
is($t3apspfw->path_length(qw(b a)), undef);
is($t3apspfw->path_length(qw(b b)), 0);
is($t3apspfw->path_length(qw(b c)), 1);
is($t3apspfw->path_length(qw(c a)), undef);
is($t3apspfw->path_length(qw(c b)), undef);
is($t3apspfw->path_length(qw(c c)), 0);

is("@{[$t3apspfw->path_vertices(qw(a a))]}", "");
is("@{[$t3apspfw->path_vertices(qw(a b))]}", "a b");
is("@{[$t3apspfw->path_vertices(qw(a c))]}", "a b c");
is("@{[$t3apspfw->path_vertices(qw(b a))]}", "");
is("@{[$t3apspfw->path_vertices(qw(b b))]}", "");
is("@{[$t3apspfw->path_vertices(qw(b c))]}", "b c");
is("@{[$t3apspfw->path_vertices(qw(c a))]}", "");
is("@{[$t3apspfw->path_vertices(qw(c b))]}", "");
is("@{[$t3apspfw->path_vertices(qw(c c))]}", "");

is($t3apspfw->path_predecessor(qw(a a)), undef);
is($t3apspfw->path_predecessor(qw(a b)), "b");
is($t3apspfw->path_predecessor(qw(a c)), "b");
is($t3apspfw->path_predecessor(qw(b a)), undef);
is($t3apspfw->path_predecessor(qw(b b)), undef);
is($t3apspfw->path_predecessor(qw(b c)), "c");
is($t3apspfw->path_predecessor(qw(c a)), undef);
is($t3apspfw->path_predecessor(qw(c b)), undef);
is($t3apspfw->path_predecessor(qw(c c)), undef);

{
    # From Andras Salamon
    use Graph;
    my $g = Graph->new;
    $g->add_edges(qw(a b b c a d d e b f));
    my $t = $g->TransitiveClosure_Floyd_Warshall; # the calling convention
    ok( $t->is_reachable('a', 'f'));
    ok(!$t->is_reachable('c', 'f'));
}

{
    # From Andras Salamon
    my $g = Graph->new;
    $g->add_edges( qw( a b b c ) );
    $g->add_vertex( 'd' );

    my $t0 = $g->TransitiveClosure_Floyd_Warshall(reflexive => 0);
    ok( $t0->has_vertex( 'a' ) );
    ok(!$t0->has_vertex( 'd' ) );

    my $t1 = $g->TransitiveClosure_Floyd_Warshall(reflexive => 1);
    ok( $t1->has_vertex( 'a' ) );
    ok( $t1->has_vertex( 'd' ) );
}

{
    # From Andras Salamon
    use Graph::Directed;
    my $g = new Graph::Directed;
    $g->add_edges( qw(a b b c) );
    is($g->APSP_Floyd_Warshall, 'a-a,a-b,a-c,b-b,b-c,c-c');
}

{
    # From Nathan Goodman.
    my $graph=new Graph::Directed;
    $graph->add_weighted_edge(0,1,1);
    $graph->add_weighted_edge(1,2,1);

    my $tc1=new Graph::TransitiveClosure($graph);

    is ("@{[sort $tc1->path_vertices(0,1)]}", "0 1");
    is ("@{[sort $tc1->path_vertices(0,2)]}", "0 1 2");
    is ("@{[sort $tc1->path_vertices(1,2)]}", "1 2");

    my $tc2=new Graph::TransitiveClosure($graph,path_length=>1,path_vertices=>1);

    is ("@{[sort $tc2->path_vertices(0,1)]}", "0 1");
    is ("@{[sort $tc2->path_vertices(0,2)]}", "0 1 2");
    is ("@{[sort $tc2->path_vertices(1,2)]}", "1 2");

}
