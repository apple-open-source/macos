use Test::More tests => 5;

use Graph;
my $h = Graph->new(hypervertexed => 1);

ok( !$h->has_edges() );

my $v = $h->edges;

is( $v, 0 );

my @v = $h->edges;

is( "@v", "" );

$h->add_edge(["a","b"],"c");
$h->add_edge("a",["b","c"]);
$h->add_edge(["a","b","c"],[]);
$h->add_edge([],["a","b","c"]);

my $e = $h->edges;

is( $e, 4 );

sub deref {
    my $r = shift;
    ref $r ? "[" . join(" ", map { deref($_) } @$r) . "]" : $_;
}

my @e = sort map { deref($_) } $h->edges;

is( "@e", "[[] [a b c]] [[a b c] []] [[a b] c] [a [b c]]" );

