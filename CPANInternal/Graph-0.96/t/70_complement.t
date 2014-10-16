use Test::More tests => 29;

use Graph;
use Graph::Directed;
use Graph::Undirected;

my $g = Graph->new;
my $c = $g->complement;

is($c->edges, 0);

my $g0 = Graph::Directed->new();
$g0->add_edge(qw(a b));
$g0->add_edge(qw(a c));

my $g1 = Graph::Undirected->new();
$g1->add_edge(qw(a b));
$g1->add_edge(qw(a c));

my $c0 = $g0->complement;

ok(!$c0->has_edge(qw(a b)));
ok(!$c0->has_edge(qw(a c)));
ok( $c0->has_edge(qw(b a)));
ok( $c0->has_edge(qw(b c)));
ok( $c0->has_edge(qw(c a)));
ok( $c0->has_edge(qw(c b)));

is($g0, "a-b,a-c");
is($c0, "b-a,b-c,c-a,c-b");

my $c1 = $g1->complement;

ok(!$c1->has_edge(qw(a b)));
ok(!$c1->has_edge(qw(a c)));
ok(!$c1->has_edge(qw(b a)));
ok( $c1->has_edge(qw(b c)));
ok(!$c1->has_edge(qw(c a)));
ok( $c1->has_edge(qw(c b)));

is($g1, "a=b,a=c");
is($c1, "b=c");

my $g2 = Graph::Directed->new(countedged => 1);
$g2->add_edge(qw(a b));
$g2->add_edge(qw(a c));

my $c2 = $g2->complement_graph;

for my $u (qw(a b c)) {
    for my $v (qw(a b c)) {
	next if $u eq $v;
	ok($g2->has_edge($u, $v) ^ $c2->has_edge($u, $v));
	is($c2->get_edge_count($u, $v), $c2->has_edge($u, $v) ? 1 : 0);
    }
}

