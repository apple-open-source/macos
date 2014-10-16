use Test::More tests => 12;

use Graph;
my $g = Graph->new(hypervertexed => 1);

$g->add_edge("a", "b");
$g->add_edge("b", "a");
$g->add_edge("a", ["b", "c"]);
$g->add_edge(["a", "b"], "c");
$g->add_edge(["c", "d"], "e");
$g->add_edge("d" ,"e");
$g->add_edge(["a", "b", "c"], "d");

sub at {
    join(" ", sort map { ref $_ ? "[@$_]" : $_ } $g->vertices_at(@_));
}

is( at("a"), "[a b c] [a b] [a]");
is( at("b"), "[a b c] [a b] [b c] [b]");
is( at("c"), "[a b c] [b c] [c d] [c]");
is( at("d"), "[c d] [d]");
is( at("e"), "[e]");
is( at("x"), "");

is( at("a", "b"), "[a b c] [a b]");
is( at("b", "a"), "[a b c] [a b]");
is( at("a", "c"), "[a b c]");
is( at("a", "d"), "");

is( at("a", "b", "c"), "[a b c]");
is( at("a", "b", "d"), "");

