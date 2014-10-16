use Test::More tests => 48;

use Graph::Directed;
use Graph::Undirected;

my $g0 = Graph::Directed->new;
my $g1 = Graph::Undirected->new;
my $g2 = Graph::Directed->new;
my $g3 = Graph::Undirected->new;
my $g4 = Graph::Directed->new;
my $g5 = Graph::Undirected->new;

$g0->add_path(qw(a b c));
$g0->add_path(qw(d b e));

$g1->add_path(qw(a b c));
$g1->add_path(qw(d b e));

$g2->add_path(qw(a b c d));
$g2->add_path(qw(c a));

$g3->add_path(qw(a b c d));
$g3->add_path(qw(c a));

$g4->add_path(qw(a b c));
$g4->add_path(qw(b a));

$g5->add_path(qw(a b c));
$g5->add_path(qw(b a));

my $g0t = $g0->transpose;
my $g1t = $g1->transpose;
my $g2t = $g2->transpose;
my $g3t = $g3->transpose;
my $g4t = $g4->transpose;
my $g5t = $g5->transpose;

is("@{[sort $g0t->successors('a')]}", "");
is("@{[sort $g0t->successors('b')]}", "a d");
is("@{[sort $g0t->successors('c')]}", "b");
is("@{[sort $g0t->successors('d')]}", "");
is("@{[sort $g0t->successors('e')]}", "b");

is("@{[sort $g0t->predecessors('a')]}", "b");
is("@{[sort $g0t->predecessors('b')]}", "c e");
is("@{[sort $g0t->predecessors('c')]}", "");
is("@{[sort $g0t->predecessors('d')]}", "b");
is("@{[sort $g0t->predecessors('e')]}", "");

is("@{[sort $g1t->successors('a')]}", "b");
is("@{[sort $g1t->successors('b')]}", "a c d e");
is("@{[sort $g1t->successors('c')]}", "b");
is("@{[sort $g1t->successors('d')]}", "b");
is("@{[sort $g1t->successors('e')]}", "b");

is("@{[sort $g1t->predecessors('a')]}", "b");
is("@{[sort $g1t->predecessors('b')]}", "a c d e");
is("@{[sort $g1t->predecessors('c')]}", "b");
is("@{[sort $g1t->predecessors('d')]}", "b");
is("@{[sort $g1t->predecessors('e')]}", "b");

is("@{[sort $g2t->successors('a')]}", "c");
is("@{[sort $g2t->successors('b')]}", "a");
is("@{[sort $g2t->successors('c')]}", "b");
is("@{[sort $g2t->successors('d')]}", "c");

is("@{[sort $g2t->predecessors('a')]}", "b");
is("@{[sort $g2t->predecessors('b')]}", "c");
is("@{[sort $g2t->predecessors('c')]}", "a d");
is("@{[sort $g2t->predecessors('d')]}", "");

is("@{[sort $g3t->successors('a')]}", "b c");
is("@{[sort $g3t->successors('b')]}", "a c");
is("@{[sort $g3t->successors('c')]}", "a b d");
is("@{[sort $g3t->successors('d')]}", "c");

is("@{[sort $g3t->predecessors('a')]}", "b c");
is("@{[sort $g3t->predecessors('b')]}", "a c");
is("@{[sort $g3t->predecessors('c')]}", "a b d");
is("@{[sort $g3t->predecessors('d')]}", "c");

is("@{[sort $g4t->successors('a')]}", "b");
is("@{[sort $g4t->successors('b')]}", "a");
is("@{[sort $g4t->successors('c')]}", "b");

is("@{[sort $g4t->predecessors('a')]}", "b");
is("@{[sort $g4t->predecessors('b')]}", "a c");
is("@{[sort $g4t->predecessors('c')]}", "");

is("@{[sort $g5t->successors('a')]}", "b");
is("@{[sort $g5t->successors('b')]}", "a c");
is("@{[sort $g5t->successors('c')]}", "b");

is("@{[sort $g5t->predecessors('a')]}", "b");
is("@{[sort $g5t->predecessors('b')]}", "a c");
is("@{[sort $g5t->predecessors('c')]}", "b");

