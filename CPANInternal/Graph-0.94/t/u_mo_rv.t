use Graph;

use Test::More tests => 22;

my $g = Graph->new(refvertexed => 1);

my $a = \"a";
my $b = \"b";
my $c = \"c";
my $d = \"d";

$g->add_edge($a, $b);
$g->add_edge($b, $c);
$g->add_edge($b, $d);

my @s = $g->all_successors($a);
is(@s, 3);
for my $s (@s) {
    ok($s == $b || $s == $c || $s == $d);
}

my @u = $g->all_predecessors($d);
is(@u, 2);
for my $u (@u) {
    ok($u == $a || $u == $b);
}

use Math::Complex;  # Math::Complex has stringification overload

my $h = Graph->new(refvertexed => 1);

my $i = Math::Complex->new(2, 3);
my $j = Math::Complex->new(4, 5);
my $k = Math::Complex->new(6, 7);
my $l = Math::Complex->new(8, 9);

$h->add_edge($i, $j);
$h->add_edge($j, $k);
$h->add_edge($j, $l);

my @t = $h->all_successors($i);
is(@t, 3);
for my $t (@t) {
    ok($t == $j || $t == $k || $t == $l);
}

my @v = $h->all_predecessors($l);
is(@v, 2);
for my $v (@v) {
    ok($v == $i || $v == $j);
}

my $w = Graph->new(undirected => 1, refvertexed => 1);

$w->add_edge($i, $j);
$w->add_edge($j, $k);
$w->add_edge($j, $l);

my @x = $w->all_neighbors($l);
is(@x, 3);
for my $x (@x) {
    ok($x == $i || $x == $j || $x == $k);
}

my $y = Graph->new(directed => 1, refvertexed => 1);

$y->add_edge($i, $j);
$y->add_edge($j, $k);
$y->add_edge($j, $l);

my @z = $y->all_neighbors($l);
is(@z, 3);
for my $z (@z) {
    ok($z == $i || $z == $j || $z == $k);
}

