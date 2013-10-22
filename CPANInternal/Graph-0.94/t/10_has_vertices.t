use strict;

use Test::More tests => 3;

use Graph;
my $g = Graph->new;

ok( !$g->has_vertices() );

$g->add_vertex("a");

ok( $g->has_vertices() );

$g->add_vertex("b");

ok( $g->has_vertices() );

