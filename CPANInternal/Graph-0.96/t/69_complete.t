use Test::More tests => 29;

use Graph;
use Graph::Directed;
use Graph::Undirected;

my $g = Graph->new;
my $c = $g->complete;

is($c->edges, 0);

my $g0 = Graph::Directed->new();
$g0->add_edge(qw(a b));
$g0->add_edge(qw(a c));

my $g1 = Graph::Undirected->new();
$g1->add_edge(qw(a b));
$g1->add_edge(qw(a c));

my $c0 = $g0->complete;

ok($c0->has_edge(qw(a b)));
ok($c0->has_edge(qw(a c)));
ok($c0->has_edge(qw(b a)));
ok($c0->has_edge(qw(b c)));
ok($c0->has_edge(qw(c a)));
ok($c0->has_edge(qw(c b)));

is($g0, "a-b,a-c");
is($c0, "a-b,a-c,b-a,b-c,c-a,c-b");

my $c1 = $g1->complete;

ok($c1->has_edge(qw(a b)));
ok($c1->has_edge(qw(a c)));
ok($c1->has_edge(qw(b a)));
ok($c1->has_edge(qw(b c)));
ok($c1->has_edge(qw(c a)));
ok($c1->has_edge(qw(c b)));

is($g1, "a=b,a=c");
is($c1, "a=b,a=c,b=c");

my $g2 = Graph::Directed->new(countedged => 1);
$g2->add_edge(qw(a b));
$g2->add_edge(qw(a c));

my $c2 = $g2->complete_graph;

for my $u (qw(a b c)) {
    for my $v (qw(a b c)) {
	next if $u eq $v;
	ok($c2->has_edge($u, $v));
	is($c2->get_edge_count($u, $v), 1);
    }
}

