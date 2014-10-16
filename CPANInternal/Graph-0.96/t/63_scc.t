use Test::More tests => 57;

use Graph;
use Graph::Undirected;

my $g0 = Graph->new;

$g0->add_cycle(qw(a b));
$g0->add_edge(qw(b c));

my @c0 = $g0->strongly_connected_components;

is(@c0, 2);
@c0 = sort { @$a <=> @$b } @c0;
is("@{$c0[0]}", 'c');
is("@{[sort @{$c0[1]}]}", 'a b');

is($g0->strongly_connected_graph, "a+b-c");

ok(!$g0->is_strongly_connected);

my $g1 = Graph->new;

$g1->add_path(qw(f f b a c b));
$g1->add_path(qw(c e d e g h g));
$g1->add_path(qw(f d));

my @c1 = $g1->strongly_connected_components;

is(@c1, 4);
@c1 = sort { @$a <=> @$b } @c1;
is("@{[sort @{$c1[0]}]}", 'f');
is("@{[sort @{$c1[1]}]}", 'd e');
is("@{[sort @{$c1[2]}]}", 'g h');
is("@{[sort @{$c1[3]}]}", 'a b c');

my $g1s = $g1->strongly_connected_graph;

is($g1s, "a+b+c-d+e,d+e-g+h,f-a+b+c,f-d+e");

is("@{[sort @{$g1s->get_vertex_attribute('a+b+c', 'subvertices')}]}",
   "a b c");
is("@{[sort @{$g1s->get_vertex_attribute('d+e', 'subvertices')}]}",
   "d e");
is("@{[sort @{$g1s->get_vertex_attribute('f', 'subvertices')}]}",
   "f");
is("@{[sort @{$g1s->get_vertex_attribute('g+h', 'subvertices')}]}",
   "g h");
is($g1s->get_vertex_attribute('h+g', 'subvertices'), undef);

ok(!$g1->is_strongly_connected);

my $g2 = Graph->new;

$g2->add_cycle(qw(a b c));
$g2->add_cycle(qw(a d e));

my @c2 = $g2->strongly_connected_components;

is(@c2, 1);
@c2 = sort { @$a <=> @$b } @c2;
is("@{[sort @{$c2[0]}]}", 'a b c d e');

is($g2->strongly_connected_graph, "a+b+c+d+e");

ok($g2->is_strongly_connected);

my $g3 = Graph->new;

$g3->add_path(qw(a b c));
$g3->add_vertices(qw(d e f));

my @c3 = $g3->strongly_connected_components;

is(@c3, 6);
@c3 = sort { @$a <=> @$b || "@$a" cmp "@$b" } @c3;
is("@{[sort @{$c3[0]}]}", 'a');
is("@{[sort @{$c3[1]}]}", 'b');
is("@{[sort @{$c3[2]}]}", 'c');
is("@{[sort @{$c3[3]}]}", 'd');
is("@{[sort @{$c3[4]}]}", 'e');
is("@{[sort @{$c3[5]}]}", 'f');

is($g3->strongly_connected_graph, "a-b,b-c,d,e,f");

ok(!$g3->is_strongly_connected);

$g3->add_cycle('d', 'a');
$g3->add_cycle('e', 'f');

is($g3->strongly_connected_graph(hypervertex =>
				 sub { my @v = sort @{ $_[0] };
				       "(" . join(",", @v) . ")" } ),
   "(a,d)-(b),(b)-(c),(e,f)");

is($g3->strongly_connected_graph(super_component =>
				 sub { my @v = sort @_;
				       "(" . join("|", @v) . ")" } ),
   "(a|d)-(b),(b)-(c),(e|f)");

eval '$g3->strongly_connected_graph(foobar => 1)';
like($@, qr/Graph::strongly_connected_graph: Unknown option: 'foobar' /);

# Example from Sedgewick Algorithms in C Third Edition 19.1 Figure 19.8 (p 150)
my $g4 = Graph->new;
$g4->add_edges([ 0,  1], [ 0,  5], [0,  6]);
$g4->add_edges([ 2,  0], [ 2,  3]);
$g4->add_edges([ 3,  2], [ 3,  5]);
$g4->add_edges([ 4,  2], [ 4,  3], [4, 11]);
$g4->add_edges([ 5,  4]);
$g4->add_edges([ 6,  4], [ 6,  9]);
$g4->add_edges([ 7,  6], [ 7,  8]);
$g4->add_edges([ 8,  7], [ 8,  9]);
$g4->add_edges([ 9, 10], [ 9, 11]);
$g4->add_edges([10, 12]);
$g4->add_edges([11, 12]);
$g4->add_edges([12,  9]);
my @g4s = sort { $a->[0] <=> $b->[0] } map { [sort { $a <=> $b} @$_] } $g4->strongly_connected_components;
is(@g4s, 4);
is("@{$g4s[0]}", "0 2 3 4 5 6");
is("@{$g4s[1]}", "1");
is("@{$g4s[2]}", "7 8");
is("@{$g4s[3]}", "9 10 11 12");

ok( $g4->same_strongly_connected_components('0',  '2'));
ok( $g4->same_strongly_connected_components('0',  '6'));
ok(!$g4->same_strongly_connected_components('0',  '1'));
ok( $g4->same_strongly_connected_components('7',  '8'));
ok( $g4->same_strongly_connected_components('9', '10'));
ok( $g4->same_strongly_connected_components('9', '12'));
ok(!$g4->same_strongly_connected_components('0',  '7'));
ok(!$g4->same_strongly_connected_components('0',  '9'));

is( $g4->strongly_connected_component_by_vertex('0'),
    $g4->strongly_connected_component_by_vertex('2'));

isnt($g4->strongly_connected_component_by_vertex('0'),
     $g4->strongly_connected_component_by_vertex('1'));

my @s = $g4->strongly_connected_components();
is( "@{[ sort $g4->strongly_connected_component_by_index(0) ]}",
    "@{[ sort @{ $s[0] } ]}" );
is( "@{[ sort $g4->strongly_connected_component_by_index(1) ]}",
    "@{[ sort @{ $s[1] } ]}" );
is( "@{[ sort $g4->strongly_connected_component_by_index(2) ]}",
    "@{[ sort @{ $s[2] } ]}" );
is( "@{[ sort $g4->strongly_connected_component_by_index(3) ]}",
    "@{[ sort @{ $s[3] } ]}" );
is( $g4->strongly_connected_component_by_index(4),
    undef );

my $g5 = Graph::Undirected->new;

eval '$g5->strongly_connected_components';
like($@, qr/Graph::strongly_connected_components: expected directed graph, got undirected/);

eval '$g5->strongly_connected_component_by_vertex';
like($@, qr/Graph::strongly_connected_component_by_vertex: expected directed graph, got undirected/);

eval '$g5->strongly_connected_component_by_index';
like($@, qr/Graph::strongly_connected_component_by_index: expected directed graph, got undirected/);

{
    # http://rt.cpan.org/NoAuth/Bug.html?id=1193
    use Graph::Directed;

    $graph = new Graph::Directed;
    $graph->add_vertex("a");
    $graph->add_vertex("b");
    $graph->add_vertex("c");
    $graph->add_edge("a","c");
    $graph->add_edge("b","c");
    $graph->add_edge("c","a");
    @cc = $graph->strongly_connected_components;
    is(@cc, 2);
}
