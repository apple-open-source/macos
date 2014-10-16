use Test::More tests => 85;

use Graph;
my $g = Graph->new;

$g->add_vertex("a");

ok( !$g->has_vertex_attributes("a") );
ok( !$g->has_vertex_attributes("a") );

ok( $g->set_vertex_attribute("a", "color", "red") );

ok( $g->has_vertex_attribute("a", "color") );
ok( $g->has_vertex_attribute("a", "color") );

ok( $g->has_vertex_attributes("a") );
ok( $g->has_vertex_attributes("a") );

is( $g->get_vertex_attribute("a", "color"),  "red" );
is( $g->get_vertex_attribute("a", "color"),  "red" );

is( $g->get_vertex_attribute("a", "colour"), undef );
is( $g->get_vertex_attribute("a", "colour"), undef );

ok( $g->set_vertex_attribute("a", "color", "green") );

ok( $g->has_vertex_attributes("a") );
ok( $g->has_vertex_attributes("a") );

is( $g->get_vertex_attribute("a", "color"),  "green" );
is( $g->get_vertex_attribute("a", "color"),  "green" );

my $attr = $g->get_vertex_attributes("a");
my @name = $g->get_vertex_attribute_names("a");
my @val  = $g->get_vertex_attribute_values("a");

is( scalar keys %$attr, 1 );
is( scalar @name,       1 );
is( scalar @val,        1 );

is( $attr->{color}, "green" );
is( $name[0],       "color" );
is( $val[0],        "green" );

ok( $g->set_vertex_attribute("a", "taste", "rhubarb") );

ok( $g->has_vertex_attributes("a") );
ok( $g->has_vertex_attributes("a") );

is( $g->get_vertex_attribute("a", "taste"),  "rhubarb" );
is( $g->get_vertex_attribute("a", "taste"),  "rhubarb" );

is( $g->get_vertex_attribute("a", "color"),  "green" );
is( $g->get_vertex_attribute("a", "taste"),  "rhubarb" );

$attr = $g->get_vertex_attributes("a");
@name = sort $g->get_vertex_attribute_names("a");
@val  = sort $g->get_vertex_attribute_values("a");

is( scalar keys %$attr, 2 );
is( scalar @name,       2 );
is( scalar @val,        2 );

is( $attr->{color}, "green" );
is( $attr->{taste}, "rhubarb" );
is( $name[0],       "color" );
is( $val[0],        "green" );
is( $name[1],       "taste" );
is( $val[1],        "rhubarb" );

ok( $g->delete_vertex_attribute("a", "color" ) );

ok( !$g->has_vertex_attribute("a", "color" ) );
ok(  $g->has_vertex_attributes("a") );
is(  $g->get_vertex_attribute("a", "taste"),  "rhubarb" );

ok(  $g->delete_vertex_attributes("a") );
ok( !$g->has_vertex_attributes("a") );
is(  $g->get_vertex_attribute("a", "taste"),  undef );

ok( !$g->delete_vertex_attribute("a", "taste" ) );
ok( !$g->delete_vertex_attributes("a") );

$attr = $g->get_vertex_attributes("a");
@name = $g->get_vertex_attribute_names("a");
@val  = $g->get_vertex_attribute_values("a");

is( scalar keys %$attr, 0 );
is( scalar @name,       0 );
is( scalar @val,        0 );

ok( $g->delete_vertex("b") );
ok(!$g->has_vertex("b"));
$g->add_weighted_vertex("b", 42);
ok( $g->has_vertex("b"));

is( $g->get_vertex_weight("b"),  42 );

is( $g->vertices, 2 );

$g->add_weighted_vertices("b", 43, "c", 44);
is( $g->get_vertex_weight("b"),  43 );
is( $g->get_vertex_weight("c"),  44 );

is( $g->vertices, 3 );

ok($g->set_vertex_attributes('a',
		             { 'color' => 'pearl', 'weight' => 'heavy' }));
$attr = $g->get_vertex_attributes('a');
is(scalar keys %$attr, 2);
is($attr->{color},  'pearl');
is($attr->{weight}, 'heavy');

ok( $g->set_vertex_weight("a", 42));
is( $g->get_vertex_weight("a"), 42);
ok( $g->has_vertex_weight("a"));
ok(!$g->has_vertex_weight("x"));
ok( $g->delete_vertex_weight("a"));
ok(!$g->has_vertex_weight("a"));
is( $g->get_vertex_weight("a"), undef);

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

    $g1a->set_vertex_attribute('b', 'color', 'electric blue');
    $g1b->set_vertex_attribute('b', 'color', 'firetruck red');

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

