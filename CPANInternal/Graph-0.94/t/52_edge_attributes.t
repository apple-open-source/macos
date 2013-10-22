use Test::More tests => 101;

use Graph;
my $g = Graph->new;

$g->add_edge("a", "b");

ok( !$g->has_edge_attributes("a", "b") );
ok( !$g->has_edge_attributes("a", "b") );

ok( $g->set_edge_attribute("a", "b", "color", "red") );

ok( $g->has_edge_attribute("a", "b", "color") );
ok( $g->has_edge_attribute("a", "b", "color") );

ok( $g->has_edge_attributes("a", "b") );
ok( $g->has_edge_attributes("a", "b") );

is( $g->get_edge_attribute("a", "b", "color"),  "red" );
is( $g->get_edge_attribute("a", "b", "color"),  "red" );

is( $g->get_edge_attribute("a", "b", "colour"), undef );
is( $g->get_edge_attribute("a", "b", "colour"), undef );

ok( $g->set_edge_attribute("a", "b", "color", "green") );

ok( $g->has_edge_attributes("a", "b") );
ok( $g->has_edge_attributes("a", "b") );

is( $g->get_edge_attribute("a", "b", "color"),  "green" );
is( $g->get_edge_attribute("a", "b", "color"),  "green" );

my $attr = $g->get_edge_attributes("a", "b");
my @name = $g->get_edge_attribute_names("a", "b");
my @val  = $g->get_edge_attribute_values("a", "b");

is( scalar keys %$attr, 1 );
is( scalar @name,       1 );
is( scalar @val,        1 );

is( $attr->{color}, "green" );
is( $name[0],       "color" );
is( $val[0],        "green" );

ok( $g->set_edge_attribute("a", "b", "taste", "rhubarb") );

ok( $g->has_edge_attributes("a", "b") );
ok( $g->has_edge_attributes("a", "b") );

is( $g->get_edge_attribute("a", "b", "taste"),  "rhubarb" );
is( $g->get_edge_attribute("a", "b", "taste"),  "rhubarb" );

is( $g->get_edge_attribute("a", "b", "color"),  "green" );
is( $g->get_edge_attribute("a", "b", "taste"),  "rhubarb" );

$attr = $g->get_edge_attributes("a", "b");
@name = sort $g->get_edge_attribute_names("a", "b");
@val  = sort $g->get_edge_attribute_values("a", "b");

is( scalar keys %$attr, 2 );
is( scalar @name,       2 );
is( scalar @val,        2 );

is( $attr->{color}, "green" );
is( $attr->{taste}, "rhubarb" );
is( $name[0],       "color" );
is( $val[0],        "green" );
is( $name[1],       "taste" );
is( $val[1],        "rhubarb" );

ok( $g->delete_edge_attribute("a", "b", "color" ) );

ok( !$g->has_edge_attribute("a", "b", "color" ) );
ok(  $g->has_edge_attributes("a", "b") );
is(  $g->get_edge_attribute("a", "b", "taste"),  "rhubarb" );

ok(  $g->delete_edge_attributes("a", "b") );
ok( !$g->has_edge_attributes("a", "b") );
is(  $g->get_edge_attribute("a", "b", "taste"),  undef );

ok( !$g->delete_edge_attribute("a", "b", "taste" ) );
ok( !$g->delete_edge_attributes("a", "b") );

$attr = $g->get_edge_attributes("a", "b");
@name = $g->get_edge_attribute_names("a", "b");
@val  = $g->get_edge_attribute_values("a", "b");

is( scalar keys %$attr, 0 );
is( scalar @name,       0 );
is( scalar @val,        0 );

ok( $g->delete_edge("c", "d") );
ok(!$g->has_edge("c", "d"));
$g->add_weighted_edge("c", "d", 42);
ok( $g->has_edge("c", "d") );

is( $g->get_edge_attribute("c", "d", "weight"),  42 );

is( $g->edges, 2 );

ok( $g->delete_edge("c", "d") );
ok( $g->delete_edge("e", "f") );
ok(!$g->has_edge("c", "d"));
ok(!$g->has_edge("e", "f"));
$g->add_weighted_edges("c", "d", 43, "e", "f", 44);
ok( $g->has_edge("c", "d") );
ok( $g->has_edge("e", "f") );
is( $g->get_edge_weight("c", "d"),  43 );
is( $g->get_edge_weight("e", "f"),  44 );

is( $g->edges, 3 );

ok( $g->delete_edge("c", "d") );
ok( $g->delete_edge("d", "e") );
$g->add_weighted_path("c", 45, "d", 46, "e");
ok( $g->has_edge("c", "d") );
ok( $g->has_edge("d", "e") );
is( $g->get_edge_weight("c", "d"),  45 );
is( $g->get_edge_weight("d", "e"),  46 );

is( $g->edges, 4 );

use Graph::Undirected;
my $u = Graph::Undirected->new;

$u->add_weighted_edge('a', 'b', 123);

is($u->get_edge_weight('a', 'b'), 123);
is($u->get_edge_weight('b', 'a'), 123);

ok($u->set_edge_attributes('a', 'b',
		           { 'color' => 'pearl', 'weight' => 'heavy' }));
$attr = $u->get_edge_attributes('a', 'b');
is(scalar keys %$attr, 2);
is($attr->{color},  'pearl');
is($attr->{weight}, 'heavy');

ok( $g->set_edge_weight("a", "b", 42));
is( $g->get_edge_weight("a", "b"), 42);
ok( $g->has_edge_weight("a", "b"));
ok(!$g->has_edge_weight("a", "c"));
ok( $g->delete_edge_weight("a", "b"));
ok(!$g->has_edge_weight("a", "b"));
is( $g->get_edge_weight("a", "b"), undef);

my $v = Graph::Undirected->new;
$v->add_weighted_path("b", 1, "f",
		           2, "c",
		           3, "d",
		           3, "f",
		           2, "g",
		           2, "e");
ok( $v, "b=f,c=d,c=f,d=f,e=g,f=g" );

{
    use Graph::Directed;
    use Graph::Undirected;

    my $g1a = Graph::Directed->new;
    my $g1b = Graph::Undirected->new;

    $g1a->add_edge(qw(a b));
    $g1a->add_edge(qw(b c));
    $g1a->add_edge(qw(b d));

    $g1b->add_edge(qw(a b));
    $g1b->add_edge(qw(b c));
    $g1b->add_edge(qw(b d));
    
    $g1a->set_edge_attribute('b', 'c', 'color', 'electric blue');
    $g1b->set_edge_attribute('b', 'c', 'color', 'firetruck red');

    is("@{[sort $g1a->successors('a')]}",   "b");
    is("@{[sort $g1a->successors('b')]}",   "c d");
    is("@{[sort $g1a->successors('c')]}",   "");
    is("@{[sort $g1a->successors('d')]}",   "");
    is("@{[sort $g1a->predecessors('a')]}", "");
    is("@{[sort $g1a->predecessors('b')]}", "a");
    is("@{[sort $g1a->predecessors('c')]}", "b");
    is("@{[sort $g1a->predecessors('d')]}", "b");

    is("@{[sort $g1b->successors('a')]}",   "b");
    is("@{[sort $g1b->successors('b')]}",   "a c d");
    is("@{[sort $g1b->successors('c')]}",   "b");
    is("@{[sort $g1b->successors('d')]}",   "b");
    is("@{[sort $g1b->predecessors('a')]}", "b");
    is("@{[sort $g1b->predecessors('b')]}", "a c d");
    is("@{[sort $g1b->predecessors('c')]}", "b");
    is("@{[sort $g1b->predecessors('d')]}", "b");
}
