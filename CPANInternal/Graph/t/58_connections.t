use Test::More tests => 24;

use Graph;
my $g0 = Graph->new;
my $g1 = Graph->new(undirected => 1);

$g0->add_edge(1=>1); $g1->add_edge(1=>1);
$g0->add_edge(1=>2); $g1->add_edge(1=>2);
$g0->add_edge(1=>3); $g1->add_edge(1=>3);
$g0->add_edge(2=>4); $g1->add_edge(2=>4);
$g0->add_edge(5=>4); $g1->add_edge(5=>4);
$g0->add_vertex(6);  $g1->add_vertex(6);  

is( "@{[sort $g0->sink_vertices()]}",      "3 4" );
is( "@{[sort $g0->source_vertices()]}",    "5" );
is( "@{[sort $g0->isolated_vertices()]}",  "6" );
is( "@{[sort $g0->interior_vertices()]}",  "2" );
is( "@{[sort $g0->exterior_vertices()]}",  "3 4 5 6" );
is( "@{[sort $g0->self_loop_vertices()]}", "1" );

is( "@{[sort $g1->sink_vertices()]}",      "" );
is( "@{[sort $g1->source_vertices()]}",    "" );
is( "@{[sort $g1->isolated_vertices()]}",  "6" );
is( "@{[sort $g1->interior_vertices()]}",  "1 2 3 4 5" );
is( "@{[sort $g1->exterior_vertices()]}",  "6" );
is( "@{[sort $g1->self_loop_vertices()]}", "1" );

use Graph::Directed;
use Graph::Undirected;

$g0 = Graph::Directed->new;
$g1 = Graph::Undirected->new;

$g0->add_path(qw(a b d));
$g0->add_path(qw(b e));
$g0->add_path(qw(a c f f));
$g0->add_path(qw(g h));
$g0->add_path(qw(i i));
$g0->add_vertex(qw(j));
$g0->add_path(qw(k k l));

is("@{[sort $g0->sink_vertices]}", "d e h l");
is("@{[sort $g0->source_vertices]}", "a g");
is("@{[sort $g0->isolated_vertices]}", "j");
is("@{[sort $g0->interior_vertices]}", "b c");
is("@{[sort $g0->exterior_vertices]}", "a d e g h j l");
is("@{[sort $g0->self_loop_vertices]}", "f i k");

$g1->add_path(qw(a b d));
$g1->add_path(qw(b e));
$g1->add_path(qw(a c f f));
$g1->add_path(qw(g h));
$g1->add_path(qw(i i));
$g1->add_vertex(qw(j));
$g1->add_path(qw(k k l));

is("@{[sort $g1->sink_vertices]}", "");
is("@{[sort $g1->source_vertices]}", "");
is("@{[sort $g1->isolated_vertices]}", "j");
is("@{[sort $g1->interior_vertices]}", "a b c d e f g h k l");
is("@{[sort $g1->exterior_vertices]}", "j");
is("@{[sort $g1->self_loop_vertices]}", "f i k");


