use Test::More tests => 20;

use Graph;
my $g = Graph->new(hyperedged => 1, hypervertexed => 1);

$g->add_edge("a", "b");
$g->add_edge("b", "a");
$g->add_edge("a", ["b", "c"]);
$g->add_edge(["a", "b"], "c");
$g->add_edge(["c", "d"], "e");
$g->add_edge("d" ,"e");
$g->add_edge(["a", "b", "c"], "d");
$g->add_edge("a", "b", "c");

sub deref {
    my $r = shift;
    ref $r ? "[" . join(" ", map { deref($_) } @$r) . "]" : $_;
}

sub at {
    join(" ", sort map { deref($_) } $g->edges_at(@_));
}

is( at("a"), "[[a b c] d] [[a b] c] [a [b c]] [a b c] [a b] [b a]");
is( at("b"), "[[a b c] d] [[a b] c] [a [b c]] [a b c] [a b] [b a]");
is( at("c"), "[[a b c] d] [[a b] c] [[c d] e] [a [b c]] [a b c]");
is( at("d"), "[[a b c] d] [[c d] e] [d e]");
is( at("e"), "[[c d] e] [d e]");
is( at("x"), "");

is( at("a", "b"), "[[a b c] d] [[a b] c]");
is( at("b", "a"), "[[a b c] d] [[a b] c]");
is( at("a", "c"), "[[a b c] d]");
is( at("a", "d"), "");

is( at("a", "b", "c"), "[[a b c] d]");
is( at("a", "b", "d"), "");

{
    # [cpan #11543] self-edges reported twice in edges_at
    use Graph::Directed;
    my $g1 = new Graph::Directed();
    $g1->add_edge(0,0);
    my @e = $g1->edges_at(0);
    is(@e, 1);
    is("@{ $e[0] }", "0 0");
}

{
    my $g2 = new Graph::Directed();
    $g2->add_edge(1,1);
    $g2->add_edge(1,2);
    my @e1 = $g2->edges_at(1);
    is(@e1, 2);
    @e1[1, 0] = @e1[0, 1] if $e1[0]->[1] > $e1[1]->[1];
    is("@{ $e1[0] }", "1 1");
    is("@{ $e1[1] }", "1 2");
    my @e2 = $g2->edges_at(2);
    is(@e2, 1);
    is("@{ $e2[0] }", "1 2");
    my @e3 = $g2->edges_at(3);
    is(@e3, 0);
}
