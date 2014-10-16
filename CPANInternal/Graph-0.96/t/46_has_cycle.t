use Test::More tests => 13;

use Graph;
my $g = Graph->new;

$g->add_cycle("a", "b", "c");

ok(   $g->has_cycle("a", "b", "c") );
ok( ! $g->has_cycle("a", "c", "b") );
ok( ! $g->has_cycle("b", "a", "c") );
ok(   $g->has_cycle("b", "c", "a") );
ok(   $g->has_cycle("c", "a", "b") );
ok( ! $g->has_cycle("c", "b", "a") );

my $h = Graph->new(undirected => 1);

$h->add_cycle("a", "b", "c");

ok(   $h->has_cycle("a", "b", "c") );
ok(   $h->has_cycle("a", "c", "b") );
ok(   $h->has_cycle("b", "a", "c") );
ok(   $h->has_cycle("b", "c", "a") );
ok(   $h->has_cycle("c", "a", "b") );
ok(   $h->has_cycle("c", "b", "a") );

ok(!  $g->has_cycle());

