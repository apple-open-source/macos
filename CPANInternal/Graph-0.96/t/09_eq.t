use Test::More tests => 14;

use Graph;

my $g = Graph->new;

$g->add_vertices(qw(a b c d));
$g->add_path(qw(b c e f));

my $h = Graph->new;

$h->add_vertices(qw(a b c d));
$h->add_path(qw(b c e f));

my $i = $h->new;

$i->add_vertex(qw(g));

is($g, "b-c,c-e,e-f,a,d");
is("b-c,c-e,e-f,a,d", $g);

ok($g->eq("b-c,c-e,e-f,a,d"));

is($g, $h);
is($h, $g);

ok($g->eq($h));
ok($h->eq($g));

isnt($i, "b-c,c-e,e-f,a,d");
isnt("b-c,c-e,e-f,a,d", $i);

ok($i->ne("b-c,c-e,e-f,a,d"));

isnt($g, $i);
isnt($i, $g);

ok($g->ne($i));
ok($i->ne($g));

