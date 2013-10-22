use Test::More tests => 31;

use Graph;

my $g0 = Graph->new;
my $g1 = Graph->new;
my $g2 = Graph->new;
my $g3 = Graph->new;
my $g4 = Graph->new;

$g0->add_edges(qw(a b b c a d));
$g1->add_edges(qw(a b b c a d)); $g1->add_vertex('e');
$g2->add_edges(qw(a b b c a d b d));
$g3->add_edges(qw(a b b c b d));
$g4->add_edges(qw(a z a x x y));

ok( $g0->could_be_isomorphic($g0));
ok(!$g0->could_be_isomorphic($g1));
ok(!$g0->could_be_isomorphic($g2));
ok(!$g0->could_be_isomorphic($g3));
ok( $g0->could_be_isomorphic($g4));

ok(!$g1->could_be_isomorphic($g0));
ok( $g1->could_be_isomorphic($g1));
ok(!$g1->could_be_isomorphic($g2));
ok(!$g1->could_be_isomorphic($g3));
ok(!$g1->could_be_isomorphic($g4));

ok(!$g2->could_be_isomorphic($g0));
ok(!$g2->could_be_isomorphic($g1));
ok( $g2->could_be_isomorphic($g2));
ok(!$g2->could_be_isomorphic($g3));
ok(!$g2->could_be_isomorphic($g4));

ok(!$g3->could_be_isomorphic($g0));
ok(!$g3->could_be_isomorphic($g1));
ok(!$g3->could_be_isomorphic($g2));
ok( $g3->could_be_isomorphic($g3));
ok(!$g3->could_be_isomorphic($g4));

ok( $g4->could_be_isomorphic($g0));
ok(!$g4->could_be_isomorphic($g1));
ok(!$g4->could_be_isomorphic($g2));
ok(!$g4->could_be_isomorphic($g3));
ok( $g4->could_be_isomorphic($g4));

my $g5a = Graph->new;
my $g5b = Graph->new;

$g5a->add_edges(qw(a b a c a d));
$g5b->add_edges(qw(a x a y a z));

is($g5a->could_be_isomorphic($g5b), 6); # 3!
is($g5b->could_be_isomorphic($g5a), 6);

$g5a->add_edges(qw(e f e g));
$g5b->add_edges(qw(e t e u));

is($g5a->could_be_isomorphic($g5b), 120); # 5!
is($g5b->could_be_isomorphic($g5a), 120);

$g5a->add_edges(qw(h i h j h k));
$g5b->add_edges(qw(h i h j h k));

is($g5a->could_be_isomorphic($g5b), 80640); # 8! * 2!
is($g5b->could_be_isomorphic($g5a), 80640);

my $g6a = Graph->new;
my $g6b = Graph->new;
my $g6c = Graph->new;

$g6a->add_vertices(qw(a b c d e f));
$g6a->add_edges(qw(a b b c b d));

$g6b->add_vertices(qw(a b c d e f));
$g6b->add_edges(qw(a b b c b e));

