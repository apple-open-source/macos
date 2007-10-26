use Graph;

use Test::More tests => 385;

my $N = 5;

sub prettyn {
    join('; ',
	 map { qq[@$_] }
	      sort { my @a = @$a; my @b = @$b;
		     my $c = @b <=> @a; return $c if $c;
		     while (@a && @b) {
			 $c = (shift @a) <=> (shift @b); return $c if $c;
		     }
		     return @a - @b }
	           map { [ sort { $a <=> $b } @$_ ] }
	 @{ $_[0] });
}

sub prettya {
    join('; ',
	 map { qq[@$_] }
	      sort { my @a = @$a; my @b = @$b;
		     my $c = @b <=> @a; return $c if $c;
		     while (@a && @b) {
			 $c = (shift @a) cmp (shift @b); return $c if $c;
		     }
		     return @a - @b }
	           map { [ sort @$_ ] }
	 @{ $_[0] });
}

my $g0a = Graph->new(undirected => 1);

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0a->biconnectivity;
    is("@{[sort { $a <=> $b } defined @$ap ? @$ap : ()]}", "");
    is("@{[prettyn($bc)]}", "");
    is("@{[prettyn($br)]}", "");
}

ok(!$g0a->is_biconnected);
ok(!$g0a->is_edge_connected);
ok(!$g0a->is_edge_separable);
is("@{[sort { $a <=> $b } $g0a->articulation_points]}", "");
is("@{[prettyn([$g0a->biconnected_components])]}", "");
is("@{[prettyn([$g0a->bridges])]}", "");

my $g0b = Graph->new(undirected => 1);
$g0b->add_vertex(0);

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0b->biconnectivity;
    is("@{[sort { $a <=> $b } defined @$ap ? @$ap : ()]}", "");
    is("@{[prettyn($bc)]}", "");
    is("@{[prettyn($br)]}", "");
}

ok(!$g0b->is_biconnected);
ok(!$g0b->is_edge_connected);
ok(!$g0b->is_edge_separable);
is("@{[sort { $a <=> $b } $g0b->articulation_points]}", "");
is("@{[prettyn([$g0b->biconnected_components])]}", "");
is("@{[prettyn([$g0b->bridges])]}", "");

my $g0c = Graph->new(undirected => 1);
$g0c->add_edge(qw(a b));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0c->biconnectivity;
    is("@{[sort { $a <=> $b } defined @$ap ? @$ap : ()]}", "");
    is("@{[prettya($bc)]}", "");
    is("@{[prettya($br)]}", "a b");
}

ok(!$g0c->is_biconnected);
ok(!$g0c->is_edge_connected);
ok( $g0c->is_edge_separable);
is("@{[sort $g0c->articulation_points]}", "");
is("@{[prettya([$g0c->biconnected_components])]}", "");
is("@{[prettya([$g0c->bridges])]}", "a b");

my $g0d = Graph->new(undirected => 1);
$g0d->add_edge(qw(a b));
$g0d->add_edge(qw(b c));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0d->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b");
    is("@{[prettya($bc)]}", "");
    is("@{[prettya($br)]}", "a b; b c");
}

ok(!$g0d->is_biconnected);
ok(!$g0d->is_edge_connected);
ok( $g0d->is_edge_separable);
is("@{[sort $g0d->articulation_points]}", "b");
is("@{[prettya([$g0d->biconnected_components])]}", "");
is("@{[prettya([$g0d->bridges])]}", "a b; b c");

my $g0e = Graph->new(undirected => 1);
$g0e->add_edge(qw(a b));
$g0e->add_edge(qw(b c));
$g0e->add_edge(qw(c d));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0e->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b c");
    is("@{[prettya($bc)]}", "");
    is("@{[prettya($br)]}", "a b; b c; c d");
}

ok(!$g0e->is_biconnected);
ok(!$g0e->is_edge_connected);
ok( $g0e->is_edge_separable);
is("@{[sort $g0e->articulation_points]}", "b c");
is("@{[prettya([$g0e->biconnected_components])]}", "");
is("@{[prettya([$g0e->bridges])]}", "a b; b c; c d");

my $g0f = Graph->new(undirected => 1);

$g0f->add_cycle(qw(a b c));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0f->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "");
    is("@{[prettya($bc)]}", "a b c");
    is("@{[prettya($br)]}", "");
}

ok( $g0f->is_biconnected);
ok( $g0f->is_edge_connected);
ok(!$g0f->is_edge_separable);
is("@{[sort $g0f->articulation_points]}", "");
is("@{[prettya([$g0f->biconnected_components])]}", "a b c");
is("@{[prettya([$g0f->bridges])]}", "");

my $g0g = Graph->new(undirected => 1);

$g0g->add_cycle(qw(a b c d));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0g->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "");
    is("@{[prettya($bc)]}", "a b c d");
    is("@{[prettya($br)]}", "");
}

ok( $g0g->is_biconnected);
ok( $g0g->is_edge_connected);
ok(!$g0g->is_edge_separable);
is("@{[sort $g0g->articulation_points]}", "");
is("@{[prettya([$g0g->biconnected_components])]}", "a b c d");
is("@{[prettya([$g0g->bridges])]}", "");

my $g0h = Graph->new(undirected => 1);

$g0h->add_cycle(qw(a b c));
$g0h->add_edge(qw(b d));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0h->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b");
    is("@{[prettya($bc)]}", "a b c");
    is("@{[prettya($br)]}", "b d");
}

ok(!$g0h->is_biconnected);
ok(!$g0h->is_edge_connected);
ok( $g0h->is_edge_separable);
is("@{[sort $g0h->articulation_points]}", "b");
is("@{[prettya([$g0h->biconnected_components])]}", "a b c");
is("@{[prettya([$g0h->bridges])]}", "b d");

my $g0i = Graph->new(undirected => 1);

$g0i->add_cycle(qw(a b c));
$g0i->add_edge(qw(b d));
$g0i->add_edge(qw(d e));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0i->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b d");
    is("@{[prettya($bc)]}", "a b c");
    is("@{[prettya($br)]}", "b d; d e");
}

ok(!$g0i->is_biconnected);
ok(!$g0i->is_edge_connected);
ok( $g0i->is_edge_separable);
is("@{[sort $g0i->articulation_points]}", "b d");
is("@{[prettya([$g0i->biconnected_components])]}", "a b c");
is("@{[prettya([$g0i->bridges])]}", "b d; d e");

my $g0j = Graph->new(undirected => 1);

$g0j->add_cycle(qw(a b c));
$g0j->add_cycle(qw(b d e));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0j->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b");
    is("@{[prettya($bc)]}", "a b c; b d e");
    is("@{[prettya($br)]}", "");
}

ok(!$g0j->is_biconnected);
ok( $g0j->is_edge_connected);
ok(!$g0j->is_edge_separable);
is("@{[sort $g0j->articulation_points]}", "b");
is("@{[prettya([$g0j->biconnected_components])]}", "a b c; b d e");
is("@{[prettya([$g0j->bridges])]}", "");

my $g0k = Graph->new(undirected => 1);

$g0k->add_cycle(qw(a b c));
$g0k->add_cycle(qw(d e f));
$g0k->add_edge(qw(b d));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0k->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b d");
    is("@{[prettya($bc)]}", "a b c; d e f");
    is("@{[prettya($br)]}", "b d");
}

ok(!$g0k->is_biconnected);
ok(!$g0k->is_edge_connected);
ok( $g0k->is_edge_separable);
is("@{[sort $g0k->articulation_points]}", "b d");
is("@{[prettya([$g0k->biconnected_components])]}", "a b c; d e f");
is("@{[prettya([$g0k->bridges])]}", "b d");

my $g0l = Graph->new(undirected => 1);

$g0l->add_cycle(qw(a b c));
$g0l->add_cycle(qw(d e f));
$g0l->add_cycle(qw(g h i));
$g0l->add_edge(qw(b d));
$g0l->add_edge(qw(d g));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0l->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b d g");
    is("@{[prettya($bc)]}", "a b c; d e f; g h i");
    is("@{[prettya($br)]}", "b d; d g");
}

ok(!$g0l->is_biconnected);
ok(!$g0l->is_edge_connected);
ok( $g0l->is_edge_separable);
is("@{[sort $g0l->articulation_points]}", "b d g");
is("@{[prettya([$g0l->biconnected_components])]}", "a b c; d e f; g h i");
is("@{[prettya([$g0l->bridges])]}", "b d; d g");

my $g0m = Graph->new(undirected => 1);

$g0m->add_cycle(qw(a b c));
$g0m->add_cycle(qw(b d e));
$g0m->add_cycle(qw(b h i));

for (0..$N-1) {
    my ($ap, $bc, $br) = $g0m->biconnectivity;
    is("@{[sort defined @$ap ? @$ap : ()]}", "b");
    is("@{[prettya($bc)]}", "a b c; b d e; b h i");
    is("@{[prettya($br)]}", "");
}

ok(!$g0m->is_biconnected);
ok( $g0m->is_edge_connected);
ok(!$g0m->is_edge_separable);
is("@{[sort $g0m->articulation_points]}", "b");
is("@{[prettya([$g0m->biconnected_components])]}", "a b c; b d e; b h i");
is("@{[prettya([$g0m->bridges])]}", "");

is("@{[sort $g0m->cut_vertices]}", "b");

my $g1 = Graph->new(undirected => 1);

$g1->add_cycle(qw(0 1 2 6));
$g1->add_cycle(qw(7 8 10));
$g1->add_cycle(qw(3 4 5));
$g1->add_cycle(qw(4 9 11));
$g1->add_edge(qw(11 12));
$g1->add_edge(qw(0 5));
$g1->add_edge(qw(6 7));

for (0..2*$N-1) {
    my ($ap, $bc, $br) = $g1->biconnectivity;
    is("@{[sort { $a <=> $b } @$ap]}", "0 4 5 6 7 11");
    is("@{[prettyn($bc)]}", "0 1 2 6; 3 4 5; 4 9 11; 7 8 10");
    is("@{[prettyn($br)]}", "0 5; 6 7; 11 12");
}

my $g2 = Graph->new(undirected => 1);

$g2->add_cycle(qw(a b c));
$g2->add_cycle(qw(d e f));
$g2->add_cycle(qw(f g h));
$g2->add_edge(qw(c d));
$g2->add_edge(qw(h i));
$g2->add_edge(qw(i j));
$g2->add_edge(qw(j k));

for (0..2*$N-1) {
    my ($ap, $bc, $br) = $g2->biconnectivity;
    is("@{[sort @$ap]}", "c d f h i j");
    is("@{[prettya($bc)]}", "a b c; d e f; f g h");
    is("@{[prettya($br)]}", "c d; h i; i j; j k");
}

my $g3 = Graph->new(undirected => 1);

$g3->add_path(qw(s a e i k j i));
$g3->add_path(qw(s b a f e));
$g3->add_path(qw(b f));
$g3->add_path(qw(s c g d h l));
$g3->add_path(qw(s d));
$g3->add_path(qw(c h));

for (0..2*$N-1) {
    my ($ap, $bc, $br) = $g3->biconnectivity;
    is("@{[sort @$ap]}", "e h i s");
    is("@{[prettya($bc)]}", "a b e f s; c d g h s; i j k");
    is("@{[prettya($br)]}", "e i; h l");
}

is( $g3->biconnected_components, 3 );

my $c0a = $g3->biconnected_component_by_index(0);
my $c0b = $g3->biconnected_component_by_index(0);
my $c0c = $g3->biconnected_component_by_index(0);

my $c1a = $g3->biconnected_component_by_index(1);
my $c1b = $g3->biconnected_component_by_index(1);
my $c1c = $g3->biconnected_component_by_index(1);

my $c2a = $g3->biconnected_component_by_index(2);
my $c2b = $g3->biconnected_component_by_index(2);
my $c2c = $g3->biconnected_component_by_index(2);

is( "@$c0a", "@$c0b" );
is( "@$c0a", "@$c0c" );

is( "@$c1a", "@$c1b" );
is( "@$c1a", "@$c1c" );

is( "@$c2a", "@$c2b" );
is( "@$c2a", "@$c2c" );

isnt( "@$c0a", "@$c1a" );
isnt( "@$c0a", "@$c2a" );
isnt( "@$c1a", "@$c2a" );

my @c0a = sort @$c0a;
my @c1a = sort @$c1a;
my @c2a = sort @$c2a;

ok( (grep { $_ eq 'i' } @c0a) ||
    (grep { $_ eq 'i' } @c1a) ||
    (grep { $_ eq 'i' } @c2a) );

is( $g3->biconnected_component_by_index(3), undef );

my $g3c = $g3->biconnected_graph();

is( $g3c, "a+b+e+f+s=c+d+g+h+s,i+j+k" );

ok( $g3->same_biconnected_components('a', 'b') );
ok( $g3->same_biconnected_components('a', 'b', 'e') );
ok(!$g3->same_biconnected_components('a', 'c') );
ok(!$g3->same_biconnected_components('a', 'b', 'c') );

is("@{[sort @{ $g3c->get_vertex_attribute('a+b+e+f+s', 'subvertices') }]}", "a b e f s");
is("@{[sort @{ $g3c->get_vertex_attribute('i+j+k', 'subvertices') }]}", "i j k");
is($g3c->get_vertex_attribute('i+k+j', 'subvertices'), undef);

my $d = Graph->new;

eval '$d->biconnectivity';
like($@, qr/Graph::biconnectivity: expected undirected graph, got directed/);

