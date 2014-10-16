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

sub from {
    join(" ", sort map { "[" . join(" ", map { ref $_ ? "[@$_]" : $_ } @$_) . "]" } $g->edges_from(@_));
}

is( from("a"), "[a b]");
is( from("b"), "[b c]");
is( from("c"), "[c d] [c f] [c g]");
is( from("d"), "[d d]");
is( from("e"), "[e b]");
is( from("f"), "");
is( from("g"), "[g h]");
is( from("h"), "[h g]");
is( from("x"), "");

{
    use Graph::Directed;
    my $g1 = new Graph::Directed();
    $g1->add_edge(0,0);
    my @e = $g1->edges_from(0);
    is(@e, 1);
    is("@{ $e[0] }", "0 0");
}

{
    my $g2 = new Graph::Directed();
    $g2->add_edge(1,1);
    $g2->add_edge(1,2);
    my @e1 = $g2->edges_from(1);
    is(@e1, 2);
    @e1[1, 0] = @e1[0, 1] if $e1[0]->[1] > $e1[1]->[1];
    is("@{ $e1[0] }", "1 1");
    is("@{ $e1[1] }", "1 2");
    my @e2 = $g2->edges_from(2);
    is(@e2, 0);
    my @e3 = $g2->edges_from(0);
    is(@e3, 0);
}
