use strict;

use Test::More tests => 7;

use Graph;
my $g = Graph->new;

ok( !$g->has_vertices() );

my $v = $g->vertices;

is( $v, 0 );

my @v = $g->vertices;

is( "@v", "" );

$g->add_vertex("a");

$v = $g->vertices;

is( $v, 1 );

@v = $g->vertices;

is( "@v", "a" );

$g->add_vertex("b");

$v = $g->vertices;

is( $v, 2 );

@v = sort $g->vertices;

is( "@v", "a b" );

