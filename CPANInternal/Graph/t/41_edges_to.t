use Test::More tests => 16;

use Graph;
my $g = Graph->new;

$g->add_edge("a", "b");
$g->add_edge("b", "c");
$g->add_edge("c", "d");
$g->add_edge("d", "d");
$g->add_edge("e", "b");
$g->add_edge("c", "f");
$g->add_edge("c", "g");
$g->add_edge("g", "h");
$g->add_edge("h", "g");

sub to {
    join(" ", sort map { "[" . join(" ", map { ref $_ ? "[@$_]" : $_ } @$_) . "]" } $g->edges_to(@_));
}

is( to("a"), "");
is( to("b"), "[a b] [e b]");
is( to("c"), "[b c]");
is( to("d"), "[c d] [d d]");
is( to("e"), "");
is( to("f"), "[c f]");
is( to("g"), "[c g] [h g]");
is( to("h"), "[g h]");
is( to("x"), "");

{
    use Graph::Directed;
    my $g1 = new Graph::Directed();
    $g1->add_edge(0,0);
    my @e = $g1->edges_to(0);
    is(@e, 1);
    is("@{ $e[0] }", "0 0");
}

{
    my $g2 = new Graph::Directed();
    $g2->add_edge(1,1);
    $g2->add_edge(1,2);
    my @e1 = $g2->edges_to(1);
    is(@e1, 1);
    is("@{ $e1[0] }", "1 1");
    my @e2 = $g2->edges_to(2);
    is(@e2, 1);
    is("@{ $e2[0] }", "1 2");
    my @e3 = $g2->edges_to(3);
    is(@e3, 0);
}
