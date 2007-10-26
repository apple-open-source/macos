use Test::More tests => 12;

use Graph;
my $g = Graph->new;

$g->add_path("a", "b", "c");

ok(   $g->has_path("a", "b", "c") );
ok( ! $g->has_path("a", "c", "b") );
ok( ! $g->has_path("b", "a", "c") );
ok( ! $g->has_path("b", "c", "a") );
ok( ! $g->has_path("c", "a", "b") );
ok( ! $g->has_path("c", "b", "a") );

my $h = Graph->new(undirected => 1);

$h->add_path("a", "b", "c");

ok(   $h->has_path("a", "b", "c") );
ok( ! $h->has_path("a", "c", "b") );
ok( ! $h->has_path("b", "a", "c") );
ok( ! $h->has_path("b", "c", "a") );
ok( ! $h->has_path("c", "a", "b") );
ok(   $h->has_path("c", "b", "a") );

