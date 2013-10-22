use Test::More tests => 152;

use Graph;

my $g = Graph->new;

is($g->random_vertex,           undef);
is($g->random_edge,             undef);
is($g->random_successor('a'),   undef);
is($g->random_predecessor('a'), undef);

for my $v (0..9) {
    $g->add_edge($v, 2 * $v);
}

# print "g = $g\n";

my $N = 30;

for (1..$N) {
    my $v = $g->random_vertex();
    ok($v >= 0 && $v <= 18);
}

for (1..$N) {
    my $e = $g->random_edge();
    my ($u, $v) = @$e;
    is($v, 2 * $u);
}

for (1..$N) {
    my ($u, $v);
    do {
	$u = $g->random_vertex();
	$v = $g->random_successor($u);
    } until (defined $v);
    is($v, 2 * $u);
}

for (1..$N) {
    my ($u, $v);
    do {
	$v = $g->random_vertex();
	$u = $g->random_predecessor($v);
    } until (defined $u);
    is($v, 2 * $u);
}

my $g0 = Graph->random_graph(vertices => 30, directed => 0);
my $g1 = Graph->random_graph(vertices => 30, directed => 1);
my $g2 = Graph->random_graph(vertices => 30, edges      => 100);
my $g3 = Graph->random_graph(vertices => 30, edges_fill => 0.1);

is($g0->vertices, 30);
is($g0->edges,    218);
ok($g0->undirected);

is($g1->vertices, 30);
is($g1->edges,    435);
ok($g1->directed);

is($g2->vertices, 30);
is($g2->edges,    100);

is($g3->vertices, 30);
is($g3->edges,    44); # int(30*29/2*0.1+0.5)

my $g4a = Graph->random_graph(vertices => 10, random_seed => 1234);
my $g4b = Graph->random_graph(vertices => 10, random_seed => 1234);
my $g4c = Graph->random_graph(vertices => 10, random_seed => 1235);
my $g4d = Graph->random_graph(vertices => 10, random_seed => 1235);
my $g4e = Graph->random_graph(vertices => 10);

is  ($g4a, $g4b);
is  ($g4c, $g4d);
isnt($g4a, $g4c);
isnt($g4a, $g4d);
isnt($g4a, $g4e);
isnt($g4c, $g4e);

my $g5 = Graph->random_graph(vertices => 10,
			     edges => 10,
			     random_edge =>
			     sub {
				 my ($g, $u, $v, $p) = @_;
				 # Create two "boxes" so that vertices 0..4
				 # only have edges between each other, ditto
				 # for vertices 5..9.
				 my $a = $u < 5;
				 my $b = $v < 5;
				 return $a == $b ? $p : 0;
			     });

for my $e ($g5->edges) {
    my ($u, $v) = @$e;
    my $a = $u < 5;
    my $b = $v < 5;
    is($a, $b, "u = $u, v = $v");
}

my $g6 = Graph::random_graph(vertices => 10);

isa_ok($g6, 'Graph');
is($g6->vertices, 10);

