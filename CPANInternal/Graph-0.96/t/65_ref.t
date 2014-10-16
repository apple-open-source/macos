use Test::More tests => 886;

use Graph;
use Graph::AdjacencyMap::Heavy;

sub _REF () { Graph::AdjacencyMap::Heavy::_REF }

use Math::Complex;

my $t = [1, 2];
my $u = bless { 3, 4 }, "Ubu";
my $v = cplx(3, 4);
my $z = cplx(4, 5);

my $m1 = Graph::AdjacencyMap::Heavy->_new(_REF, 1);
my $m2 = Graph::AdjacencyMap::Heavy->_new(_REF, 2);

is( $m1->set_path($t), 0 );
my @m1 = $m1->_get_id_path( $m1->_get_path_id($t) );
is( $m1[0], $t );

is( $m2->set_path($u, $v), 0 );
my @m2 = $m2->_get_id_path( $m2->_get_path_id($u, $v) );
is( $m2[0], $u );
ok( $m2[1] == $v );		# is() doesn't work.
ok( $m2[1] ** 2 == $v ** 2 );	# is() doesn't work.

my $g = Graph->new(refvertexed => 1);

$g->add_vertex($v);
$g->add_edge($v, $z);

my @V = sort { $a->sqrt <=> $b->sqrt } $g->vertices;

is($V[0]->Re, 3);
is($V[0]->Im, 4);
is($V[1]->Re, 4);
is($V[1]->Im, 5);

ok($g->has_vertex($v));
ok($g->has_vertex($z));
ok($g->has_edge($v, $z));

$v->Re(7);
$z->Im(8);

ok($g->has_vertex($v));
ok($g->has_vertex($z));

@V = sort { $a->sqrt <=> $b->sqrt } $g->vertices;

is($V[0]->Re, 4);
is($V[0]->Im, 8);
is($V[1]->Re, 7);
is($V[1]->Im, 4);

my $x = cplx(1,2);
my $y = cplx(3,4);
$g = Graph->new(refvertexed => 1);
$g->add_edge($x,$y);
my @e = $g->edges;
is("@{$e[0]}", "1+2i 3+4i");
$x->Im(5);
is("@{$e[0]}", "1+5i 3+4i");
$e[0]->[1]->Im(6);
is("$y", "3+6i");

use vars qw($foo $bar);

my $o0;
my $o1;

my $o1a = bless \$o0, 'S';
my $o1b = bless \$o1, 'S';
{ package S; use overload '""' => sub { "s" } }

my $o2a = bless [], 'A';
my $o2b = bless [], 'A';
{ package A; use overload '""' => sub { "a" } }

my $o3a = bless {}, 'H';
my $o3b = bless {}, 'H';
{ package H; use overload '""' => sub { "h" } }

my $o4a = bless sub {}, 'C';
my $o4b = bless sub {}, 'C';
{ package C; use overload '""' => sub { "c" } }

my $o5a = bless \*STDIN{IO}, 'I';
my $o5b = bless \*STDOUT{IO}, 'I';
{ package I; use overload '""' => sub { "i" } }

my $o6a = bless \*foo, 'G';
my $o6b = bless \*bar, 'G';
{ package G; use overload '""' => sub { "g" } }

for my $i ($o1a, $o2a, $o3a, $o4a, $o5a, $o6a) {
    for my $j ($o1b, $o2b, $o3b, $o4b, $o5b, $o6b) {
	print "# i = $i, j = $j\n";

	my $g1 = Graph->new(refvertexed => 1, directed => 1);

	ok( $g1->add_edge($i, $j));
	print "# g1 = $g1\n";
	ok( $g1->has_vertex($i));
	ok( $g1->has_vertex($j));
	ok( $g1->has_edge($i, $j));
	ok( $g1->delete_vertex($i));
	print "# g1 = $g1\n";
	ok(!$g1->has_vertex($i));
	ok( $g1->has_vertex($j));
	ok(!$g1->has_edge($i, $j));
	ok(!$g1->delete_vertex($j));
	print "# g1 = $g1\n";
	ok(!$g1->has_vertex($i));
	ok(!$g1->has_vertex($j));
	ok(!$g1->has_edge($i, $j));

	my $g2 = Graph->new(refvertexed => 1, directed => 0);

	ok( $g2->add_edge($i, $j));
	print "# g2 = $g2\n";
	ok( $g2->has_vertex($i));
	ok( $g2->has_vertex($j));
	ok( $g2->has_edge($i, $j));
	ok( $g2->delete_vertex($i));
	print "# g2 = $g2\n";
	ok(!$g2->has_vertex($i));
	ok( $g2->has_vertex($j));
	ok(!$g2->has_edge($i, $j));
	ok(!$g2->delete_vertex($j));
	print "# g2 = $g2\n";
	ok(!$g2->has_vertex($i));
	ok(!$g2->has_vertex($j));
	ok(!$g2->has_edge($i, $j));
    }
}

