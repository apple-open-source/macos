use Test::More tests => 96;

use Graph;

my ($g0, $g1, $g2, $g3, $g4, $g5, $g6, $g7,
    $g8, $g9, $ga, $gb, $gc, $gd, $ge, $gf);

$g0 = Graph->new;
$g1 = Graph->new(countedged => 1);
$g2 = Graph->new->add_edge(qw(a a));
$g3 = Graph->new(countedged => 1)->add_edge(qw(a a));
$g4 = Graph->new->add_edge(qw(a b));
$g5 = Graph->new(countedged => 1)->add_edge(qw(a b));
$g6 = Graph->new->add_edge(qw(a a))->add_edge(qw(a b));
$g7 = Graph->new(countedged => 1)->add_edge(qw(a a))->add_edge(qw(a b));
$g8 = Graph->new->add_edge(qw(a b));
$g9 = Graph->new(countedged => 1)->add_edge(qw(a b));
$ga = Graph->new->add_edge(qw(a a))->add_edge(qw(a b));
$gb = Graph->new(countedged => 1)->add_edge(qw(a a))->add_edge(qw(a b));
$gc = Graph->new->add_edge(qw(a b))->add_edge(qw(a b));
$gd = Graph->new(countedged => 1)->add_edge(qw(a b))->add_edge(qw(a b));
$ge = Graph->new->add_edge(qw(a a))->add_edge(qw(a b))->add_edge(qw(a b));
$gf = Graph->new(countedged => 1)->add_edge(qw(a a))->add_edge(qw(a b))->add_edge(qw(a b));

ok( $g0->is_simple_graph);
ok(!$g0->is_pseudo_graph);
ok(!$g0->is_multi_graph);

ok( $g1->is_simple_graph);
ok(!$g1->is_pseudo_graph);
ok(!$g1->is_multi_graph);

ok( $g2->is_simple_graph);
ok( $g2->is_pseudo_graph); # a a
ok(!$g2->is_multi_graph);

ok( $g3->is_simple_graph);
ok( $g3->is_pseudo_graph); # a a
ok(!$g3->is_multi_graph);

ok( $g4->is_simple_graph);
ok(!$g4->is_pseudo_graph);
ok(!$g4->is_multi_graph);

ok( $g5->is_simple_graph);
ok(!$g5->is_pseudo_graph);
ok(!$g5->is_multi_graph);

ok( $g6->is_simple_graph);
ok( $g6->is_pseudo_graph); # a a once
ok(!$g6->is_multi_graph);

ok( $g7->is_simple_graph);
ok( $g7->is_pseudo_graph); # a a once
ok(!$g7->is_multi_graph);

ok( $g8->is_simple_graph);
ok(!$g8->is_pseudo_graph);
ok(!$g8->is_multi_graph);

ok( $g9->is_simple_graph);
ok(!$g9->is_pseudo_graph);
ok(!$g9->is_multi_graph);

ok( $ga->is_simple_graph);
ok( $ga->is_pseudo_graph); # a a once
ok(!$ga->is_multi_graph);

ok( $gb->is_simple_graph);
ok( $gb->is_pseudo_graph); # a a once
ok(!$gb->is_multi_graph);

ok( $gc->is_simple_graph);
ok(!$gc->is_pseudo_graph);
ok(!$gc->is_multi_graph);

ok(!$gd->is_simple_graph); # a b twice
ok( $gd->is_pseudo_graph); # a b twice
ok( $gd->is_multi_graph);  # a b twice

ok( $ge->is_simple_graph);
ok( $ge->is_pseudo_graph); # a a once
ok(!$ge->is_multi_graph);

ok(!$gf->is_simple_graph);
ok( $gf->is_pseudo_graph); # a a once, a b twice
ok(!$gf->is_multi_graph);  # a a once, a b twice

$g0 = Graph->new;
$g1 = Graph->new(multiedged => 1);
$g2 = Graph->new->add_edge(qw(a a));
$g3 = Graph->new(multiedged => 1)->add_edge(qw(a a));
$g4 = Graph->new->add_edge(qw(a b));
$g5 = Graph->new(multiedged => 1)->add_edge(qw(a b));
$g6 = Graph->new->add_edge(qw(a a))->add_edge(qw(a b));
$g7 = Graph->new(multiedged => 1)->add_edge(qw(a a))->add_edge(qw(a b));
$g8 = Graph->new->add_edge(qw(a b));
$g9 = Graph->new(multiedged => 1)->add_edge(qw(a b));
$ga = Graph->new->add_edge(qw(a a))->add_edge(qw(a b));
$gb = Graph->new(multiedged => 1)->add_edge(qw(a a))->add_edge(qw(a b));
$gc = Graph->new->add_edge(qw(a b))->add_edge(qw(a b));
$gd = Graph->new(multiedged => 1)->add_edge(qw(a b))->add_edge(qw(a b));
$ge = Graph->new->add_edge(qw(a a))->add_edge(qw(a b))->add_edge(qw(a b));
$gf = Graph->new(multiedged => 1)->add_edge(qw(a a))->add_edge(qw(a b))->add_edge(qw(a b));

ok( $g0->is_simple_graph);
ok(!$g0->is_pseudo_graph);
ok(!$g0->is_multi_graph);

ok( $g1->is_simple_graph);
ok(!$g1->is_pseudo_graph);
ok(!$g1->is_multi_graph);

ok( $g2->is_simple_graph);
ok( $g2->is_pseudo_graph); # a a
ok(!$g2->is_multi_graph);

ok( $g3->is_simple_graph);
ok( $g3->is_pseudo_graph); # a a
ok(!$g3->is_multi_graph);

ok( $g4->is_simple_graph);
ok(!$g4->is_pseudo_graph);
ok(!$g4->is_multi_graph);

ok( $g5->is_simple_graph);
ok(!$g5->is_pseudo_graph);
ok(!$g5->is_multi_graph);

ok( $g6->is_simple_graph);
ok( $g6->is_pseudo_graph); # a a once
ok(!$g6->is_multi_graph);

ok( $g7->is_simple_graph);
ok( $g7->is_pseudo_graph); # a a once
ok(!$g7->is_multi_graph);

ok( $g8->is_simple_graph);
ok(!$g8->is_pseudo_graph);
ok(!$g8->is_multi_graph);

ok( $g9->is_simple_graph);
ok(!$g9->is_pseudo_graph);
ok(!$g9->is_multi_graph);

ok( $ga->is_simple_graph);
ok( $ga->is_pseudo_graph); # a a once
ok(!$ga->is_multi_graph);

ok( $gb->is_simple_graph);
ok( $gb->is_pseudo_graph); # a a once
ok(!$gb->is_multi_graph);

ok( $gc->is_simple_graph);
ok(!$gc->is_pseudo_graph);
ok(!$gc->is_multi_graph);

ok(!$gd->is_simple_graph); # a b twice
ok( $gd->is_pseudo_graph); # a b twice
ok( $gd->is_multi_graph);  # a b twice

ok( $ge->is_simple_graph);
ok( $ge->is_pseudo_graph); # a a once
ok(!$ge->is_multi_graph);

ok(!$gf->is_simple_graph);
ok( $gf->is_pseudo_graph); # a a once, a b twice
ok(!$gf->is_multi_graph);  # a a once, a b twice
