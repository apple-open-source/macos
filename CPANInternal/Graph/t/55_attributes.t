use Test::More tests => 35;

use Graph;

{
    my $g = Graph->new;
    $g->add_weighted_vertex('a', 3);
    is($g, "a");
    is($g->get_vertex_weight('a'), 3);
}

{
    my $g = Graph->new;
    $g->add_vertex('x');
    $g->add_weighted_vertex('a', 3);
    is($g, "a,x");
    is($g->get_vertex_weight('a'), 3);
    is($g->get_vertex_weight('x'), undef);
    ok($g->has_vertex('a'));
    ok($g->has_vertex('x'));
}

{
    my $g = Graph->new;
    $g->add_edge('x', 'y');
    $g->add_weighted_edge('b', 'c', 4);
    is($g, "b-c,x-y");
    is($g->get_edge_weight('b', 'c'), 4);
    is($g->get_edge_weight('x', 'y'), undef);
    ok($g->has_vertex('b'));
    ok($g->has_vertex('c'));
    ok($g->has_edge('b', 'c'));
    ok($g->has_vertex('x'));
    ok($g->has_vertex('y'));
    ok($g->has_edge('x', 'y'));
}

{
    my $g = Graph->new;
    $g->add_weighted_edge('b', 'c', 4);
    is($g, "b-c");
    is($g->get_edge_weight('b', 'c'), 4);
    ok($g->has_vertex('b'));
    ok($g->has_vertex('c'));
    ok($g->has_edge('b', 'c'));
}

{
    my $g = Graph->new;
    $g->add_vertex('a');
    $g->add_vertex('b');
    $g->add_vertex('c');
    $g->add_weighted_vertex('x', 3);
    is($g, "a,b,c,x");
    is($g->get_vertex_weight('a'), undef);
    is($g->get_vertex_weight('b'), undef);
    is($g->get_vertex_weight('c'), undef);
    is($g->get_vertex_weight('x'), 3);
}

{
    my $g = Graph->new;
    $g->add_edge('a', 'b');
    $g->add_edge('b', 'c');
    $g->add_edge('c', 'a');
    $g->add_weighted_edge('c', 'b', 4);
    is($g, "a-b,b-c,c-a,c-b");
    ok($g->has_vertex('a'));
    ok($g->has_vertex('b'));
    ok($g->has_vertex('c'));
    ok($g->has_edge('a', 'b'));
    ok($g->has_edge('b', 'c'));
    ok($g->has_edge('c', 'a'));
    ok($g->has_edge('c', 'b'));
}

{
    my $g = Graph->new;
    $g->add_vertex('a');
    $g->add_vertex('b');
    $g->add_vertex('c');
    $g->add_edge('a', 'b');
    $g->add_edge('a', 'c');
    $g->add_edge('c', 'd');
    $g->delete_vertex('b');
    $g->add_edge('a', 'b');
    $g->add_weighted_vertex('e', 4);
    is($g, "a-b,a-c,c-d,e");
}
