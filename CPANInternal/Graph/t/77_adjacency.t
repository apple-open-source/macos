use Test::More tests => 14;

use Graph;
use Graph::AdjacencyMatrix;

my $g = Graph->new(vertices => [0..9]);

$g->add_edge(qw(2 3));

my $m = Graph::AdjacencyMatrix->new($g);

my $am = $m->adjacency_matrix;
my $dm = $m->distance_matrix;
my @V  = $m->vertices;

# use Data::Dumper; print Dumper($am);
# use Data::Dumper; print Dumper($dm);
# use Data::Dumper; print Dumper(\@V);

is(@{$am->[0]}, 10);
is($am->vertices, 10);
is("@{[sort $am->vertices]}", "0 1 2 3 4 5 6 7 8 9");

is($dm, undef);

is(@V, 10);
is("@{[sort @V]}", "0 1 2 3 4 5 6 7 8 9");

ok( $m->is_adjacent(2, 3));
ok(!$m->is_adjacent(3, 2));

is( $m->distance(2, 3), undef);
is( $m->distance(3, 2), undef);

$g->add_weighted_edge(2, 3, 45);

$m = Graph::AdjacencyMatrix->new($g, distance_matrix => 0);

is( $m->distance(2, 3), undef);
is( $m->distance(3, 2), undef);

$m = Graph::AdjacencyMatrix->new($g, distance_matrix => 1);

is( $m->distance(2, 3), 45);
is( $m->distance(3, 2), undef);
