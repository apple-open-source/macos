use Test::More tests => 49;

use Graph;

my $g0 = Graph->new;

my $g1a = Graph->new(refvertexed => 1);
my $g1b = Graph->new(refvertexed => 0);

ok( !$g0 ->refvertexed() );
ok(  $g1a->refvertexed() );
ok( !$g1b->refvertexed() );

my $g2a = Graph->new(countvertexed => 1);
my $g2b = Graph->new(countvertexed => 0);

ok( !$g0 ->countvertexed() );
ok(  $g2a->countvertexed() );
ok( !$g2b->countvertexed() );

my $g3a = Graph->new(hypervertexed => 1);
my $g3b = Graph->new(hypervertexed => 0);

ok( !$g0 ->hypervertexed() );
ok(  $g3a->hypervertexed() );
ok( !$g3b->hypervertexed() );

my $g4a = Graph->new(omnidirected => 1);
my $g4b = Graph->new(omnidirected => 0);

ok( !$g0 ->omnidirected() );
ok(  $g4a->omnidirected() );
ok( !$g4b->omnidirected() );

ok(  $g4a->undirected() );
ok( !$g4b->undirected() );

ok( !$g4a->directed() );
ok(  $g4b->directed() );

my $g5a = Graph->new(undirected => 1);
my $g5b = Graph->new(undirected => 0);

ok(  $g5a->omnidirected() );
ok( !$g5b->omnidirected() );

ok( !$g0 ->undirected() );
ok(  $g5a->undirected() );
ok( !$g5b->undirected() );

ok( !$g5a->directed() );
ok(  $g5b->directed() );

my $g6a = Graph->new(directed => 1);
my $g6b = Graph->new(directed => 0);

ok( !$g6a->omnidirected() );
ok(  $g6b->omnidirected() );

ok( !$g6a->undirected() );
ok(  $g6b->undirected() );

ok(  $g0 ->directed() ); # The default is directed.
ok(  $g6a->directed() );
ok( !$g6b->directed() );

my $g7a = Graph->new(countedged => 1);
my $g7b = Graph->new(countedged => 0);

ok( !$g0 ->countedged() );
ok(  $g7a->countedged() );
ok( !$g7b->countedged() );

{
    local $SIG{__DIE__} = sub { $@ = shift };

    eval { my $gna = Graph->new(foobar => 1) };
    like($@, qr/Graph::new: Unknown option: 'foobar' /);

    eval { my $gna = Graph->new(foobar => 0) };
    like($@, qr/Graph::new: Unknown option: 'foobar' /);

    eval { my $gna = Graph->new(foobar => 1, barfoo => 1) };
    like($@, qr/Graph::new: Unknown options: 'barfoo' 'foobar' /);
}

{
    my $g = Graph->new(vertices => [0, 1, 2]);
    ok($g->has_vertex(0));
    ok($g->has_vertex(1));
    ok($g->has_vertex(2));
}

{
    my $g = Graph->new(edges => [[0, 1], [2, 3]]);
    ok($g->has_edge(0, 1));
    ok($g->has_edge(2, 3));
}

{
    my $g = Graph->new(vertices => [0], edges => [[1, 2], [2, 3]]);
    ok($g->has_vertex(0));
    ok($g->has_edge(1, 2));
    ok($g->has_edge(2, 3));
}

{
    my $g = Graph->new(compat02 => 1, hypervertexed => 1, multiedged => 1);
    my $h = $g->new; # The flags should be inherited.
    ok($h->is_compat02);
    ok($h->is_hypervertexed);
    ok($h->is_multiedged);
}

use Graph::Directed;
my $d = Graph::Directed->new;
is(ref $d, 'Graph::Directed');

use Graph::Undirected;
my $u = Graph::Undirected->new;
is(ref $u, 'Graph::Undirected');
