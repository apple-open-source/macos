use Test::More tests => 59;

use Graph;
use Graph::Directed;
use Graph::Undirected;

my $g = Graph->new(undirected => 1);

$g->add_edge(qw(e a));
$g->add_edge(qw(a r));
$g->add_edge(qw(r t));
$g->add_edge(qw(t h));
$g->add_edge(qw(h f));
$g->add_edge(qw(f r));
$g->add_edge(qw(r o));
$g->add_edge(qw(o m));
$g->add_edge(qw(m a));
$g->add_edge(qw(a b));
$g->add_edge(qw(b o));
$g->add_edge(qw(o v));
$g->add_edge(qw(v e));

is($g->diameter, 4);
is($g->longest_path,   4);
is($g->shortest_path,  1);
is($g->radius,   2);

{
    my @c = sort $g->center_vertices;
    is(@c, 1);
    is("@c", "r");
}

is($g->average_path_length(),           19 / 9);

# Note that the below are just some of the possible paths,
# for example other possible paths of length four are
# a-r-t-h-e, a-m-o-r-t, b-o-v-e-a, ...
# a-b: a-b       : 1
# a-e: a-r-o-v-e : 4
# a-f: a-r-t-h-f : 4
# a-h: a-r-t-h   : 3
# a-m: a-r-o-m   : 3
# a-o: a-r-o     : 2
# a-r: a-r       : 1
# a-t: a-r-t     : 2
# a-v: a-r-o-v   : 3
#                  23 / 9 = 2.56
is($g->average_path_length('a'),        15 / 9);
is($g->average_path_length('b'),        20 / 9);
is($g->average_path_length('c'),        undef );
is($g->average_path_length('a', undef), 15 / 9);
is($g->average_path_length('b', undef), 20 / 9);
is($g->average_path_length(undef, 'a'), 15 / 9);
is($g->average_path_length(undef, 'b'), 20 / 9);

is($g->vertex_eccentricity('a'), 3);
is($g->vertex_eccentricity('b'), 4);
is($g->vertex_eccentricity('e'), 4);
is($g->diameter, 4);
is($g->radius,   2);

{
    my @c;
    @c = sort $g->center_vertices;
    is(@c, 1);
    is("@c", "r");
    @c = sort $g->center_vertices(1);
    is(@c, 5);
    is("@c", "a f o r t");
}

sub gino {
    my $gi = $_[0];
    my $m = (sort @$gi)[0];
    for (my $i = 0; $i < @$gi && $gi->[0] ne $m; $i++) {
	push @$gi, shift @$gi;
    }
    return @$gi;
}

my $h = Graph->new(undirected => 1);

$h->add_weighted_edge(qw(a b 2.3));
$h->add_weighted_edge(qw(a c 1.7));

is($h->longest_path,   4.0);
is($h->shortest_path,  1.7);
is($h->diameter, 4.0);
is($h->radius,   2.3);

my $i = Graph::Directed->new(undirected => 1);

$i->add_edge(qw(k a));
$i->add_edge(qw(a l));
$i->add_edge(qw(l e));
$i->add_edge(qw(e v));
$i->add_edge(qw(v a));
$i->add_edge(qw(a l));
$i->add_edge(qw(l a));
$i->add_edge(qw(a n));

is($i->vertex_eccentricity('k'), 3);
is($i->vertex_eccentricity('a'), 2);
is($i->vertex_eccentricity('l'), 2);
is($i->vertex_eccentricity('e'), 3);
is($i->vertex_eccentricity('v'), 2);
is($i->vertex_eccentricity('n'), 3);

{
    my @c = sort $i->center_vertices;
    is(@c, 3);
    is("@c", "a l v");
}

my $j = Graph::Undirected->new(undirected => 1);

$j->add_edge(qw(k a));
$j->add_edge(qw(a l));
$j->add_edge(qw(l e));
$j->add_edge(qw(e v));
$j->add_edge(qw(v a));
$j->add_edge(qw(a l));
$j->add_edge(qw(l a));
$j->add_edge(qw(a n));

is($j->vertex_eccentricity('k'), 3);
is($j->vertex_eccentricity('a'), 2);
is($j->vertex_eccentricity('l'), 2);
is($j->vertex_eccentricity('e'), 3);
is($j->vertex_eccentricity('v'), 2);
is($j->vertex_eccentricity('n'), 3);

{
    my @c = sort $j->center_vertices;
    is(@c, 3);
    is("@c", "a l v");
}

my $k = Graph::Undirected->new(undirected => 1);

$k->add_edge(qw(s t));
$k->add_edge(qw(s a));
$k->add_edge(qw(s r));

is($k->vertex_eccentricity('s'), 1);
is($k->vertex_eccentricity('t'), 2);
is($k->vertex_eccentricity('a'), 2);
is($k->vertex_eccentricity('r'), 2);

{
    my @c = sort $k->center_vertices;
    is(@c, 1);
    is($c[0], 's');
}

{
    # These tests inspired by Xiaoli Zheng.

    my $g = Graph::Directed->new(undirected => 1);

    is($g->diameter, undef);

    $g->add_edge('a', 'b');
    is($g->diameter, 1);

    $g->add_edge('b', 'c');
    is($g->diameter, 2);

    $g->add_edge('c', 'd');
    is($g->diameter, 3);

    $g->add_edge('e', 'f');
    is($g->diameter, 3);

    $g->add_edge('d', 'e');
    is($g->diameter, 5);

    $g->add_edge('g', 'f');
    is($g->diameter, 6);

    $g->delete_edge('c', 'b');
    is($g->diameter, 4);

    $g->delete_edge('b', 'c');
    is($g->diameter, 4);
}

{
    my $g = Graph->new(undirected => 1);

    $g->add_edge(qw(a b));
    $g->add_edge(qw(c d));

    is($g->vertex_eccentricity('a'), Graph::Infinity);
}
