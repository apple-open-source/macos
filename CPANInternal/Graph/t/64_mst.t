use Test::More tests => 22;

use Graph::Undirected;
use Graph::Directed;

my $g0 = Graph::Undirected->new;

$g0->add_weighted_edge(qw(a b 1));
$g0->add_weighted_edge(qw(a c 2));
$g0->add_weighted_edge(qw(a d 1));
$g0->add_weighted_edge(qw(b d 2));
$g0->add_weighted_edge(qw(b e 2));
$g0->add_weighted_edge(qw(c d 2));
$g0->add_weighted_edge(qw(c f 1));
$g0->add_weighted_edge(qw(d e 1));
$g0->add_weighted_edge(qw(d f 1));
$g0->add_weighted_edge(qw(d g 2));
$g0->add_weighted_edge(qw(e g 1));

my $g1 = $g0->deep_copy;

my $g0t = $g0->MST_Kruskal;

ok($g0t->is_undirected);
is($g0t->vertices, $g0->vertices);
is($g0t->edges, $g0->vertices - 1);
is($g0t, "a=b,a=d,c=f,d=e,d=f,e=g");

$g0->add_weighted_edge(qw(c f 3));

my $g0u = $g0->MST_Kruskal;

ok($g0u->is_undirected);
is($g0u->vertices, $g0->vertices);
is($g0u->edges, $g0->vertices - 1);
ok($g0u eq "a=b,a=c,a=d,d=e,d=f,e=g" ||
   $g0u eq "a=b,a=d,c=d,d=e,d=f,e=g" ||
   $g0u eq "a=b,a=c,c=f,d=e,e=g");

my $g1t = $g1->MST_Prim;

ok($g1t->is_undirected);
is($g1t->vertices, $g0->vertices);
is($g1t->edges, $g0->vertices - 1);
ok($g1t eq "a=b,a=d,c=f,d=e,d=f,e=g" ||
   $g1t eq "a=b,a=c,a=d,d=e,d=f,e=g");

my $g1u = $g1->MST_Prim(first_root => "g");

ok($g1u->is_undirected);
is($g1u->vertices, $g0->vertices);
is($g1u->edges, $g0->vertices - 1);
ok($g1u eq "a=b,a=d,c=f,d=e,d=f,e=g" ||
   $g1u eq "a=b,a=c,a=d,d=e,d=f,e=g");

$g1->add_weighted_edge(qw(c f 3));

my $g1v = $g1->MST_Kruskal;

ok($g1v->is_undirected);
is($g1v->vertices, $g1->vertices);
is($g1v->edges, $g1->vertices - 1);
ok($g1v eq "a=b,a=c,a=d,d=e,d=f,e=g" ||
   $g1v eq "a=b,a=d,c=d,d=e,d=f,e=g");

my $g2 = Graph::Directed->new;

eval { $g2->MST_Kruskal };
like($@, qr/Graph::MST_Kruskal: expected undirected graph, got directed, /);

eval { $g2->MST_Prim };
like($@, qr/Graph::MST_Prim: expected undirected graph, got directed, /);

