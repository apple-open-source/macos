use Graph;

use Test::More tests => 2;

my $g = Graph::Undirected->new;

# example graph #1 from http://mathworld.wolfram.com/GraphRadius.html
#
#       A
#       |
#       B
#     / | \
#   C   D   E
#       |   |
#       F   G

$g->add_edge( split //, $_ )
    for qw[ AB BC BD BE CF EG ];

my $apsp = $g->all_pairs_shortest_paths;

is( $apsp->radius, 2, "radius" );

is_deeply( [$apsp->center_vertices], ["B"], "center_vertices" );
