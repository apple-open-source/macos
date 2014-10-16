use Test::More tests => 74;

use Graph;
my $g = Graph->new(multiedged => 1);

$g->add_edge_by_id("a", "b", "hot");

ok( !$g->has_edge_attributes_by_id("a", "b", "hot") );
ok( !$g->has_edge_attributes_by_id("a", "b", "hot") );

ok( $g->set_edge_attribute_by_id("a", "b", "hot", "color", "red") );

ok( $g->has_edge_attribute_by_id("a", "b", "hot", "color") );
ok( $g->has_edge_attribute_by_id("a", "b", "hot", "color") );

ok( $g->has_edge_attributes_by_id("a", "b", "hot") );
ok( $g->has_edge_attributes_by_id("a", "b", "hot") );

is( $g->get_edge_attribute_by_id("a", "b", "hot", "color"),  "red" );
is( $g->get_edge_attribute_by_id("a", "b", "hot", "color"),  "red" );

is( $g->get_edge_attribute_by_id("a", "b", "hot", "colour"), undef );
is( $g->get_edge_attribute_by_id("a", "b", "hot", "colour"), undef );

ok( $g->set_edge_attribute_by_id("a", "b", "hot", "color", "green") );

ok( $g->has_edge_attributes_by_id("a", "b", "hot") );
ok( $g->has_edge_attributes_by_id("a", "b", "hot") );

is( $g->get_edge_attribute_by_id("a", "b", "hot", "color"),  "green" );
is( $g->get_edge_attribute_by_id("a", "b", "hot", "color"),  "green" );

my $attr = $g->get_edge_attributes_by_id("a", "b", "hot");
my @name = $g->get_edge_attribute_names_by_id("a", "b", "hot");
my @val  = $g->get_edge_attribute_values_by_id("a", "b", "hot");

is( scalar keys %$attr, 1 );
is( scalar @name,       1 );
is( scalar @val,        1 );

is( $attr->{color}, "green" );
is( $name[0],       "color" );
is( $val[0],        "green" );

ok( $g->set_edge_attribute_by_id("a", "b", "hot", "taste", "rhubarb") );

ok( $g->has_edge_attributes_by_id("a", "b", "hot") );
ok( $g->has_edge_attributes_by_id("a", "b", "hot") );

is( $g->get_edge_attribute_by_id("a", "b", "hot", "taste"),  "rhubarb" );
is( $g->get_edge_attribute_by_id("a", "b", "hot", "taste"),  "rhubarb" );

is( $g->get_edge_attribute_by_id("a", "b", "hot", "color"),  "green" );
is( $g->get_edge_attribute_by_id("a", "b", "hot", "taste"),  "rhubarb" );

$attr = $g->get_edge_attributes_by_id("a", "b", "hot");
@name = sort $g->get_edge_attribute_names_by_id("a", "b", "hot");
@val  = sort $g->get_edge_attribute_values_by_id("a", "b", "hot");

is( scalar keys %$attr, 2 );
is( scalar @name,       2 );
is( scalar @val,        2 );

is( $attr->{color}, "green" );
is( $attr->{taste}, "rhubarb" );
is( $name[0],       "color" );
is( $val[0],        "green" );
is( $name[1],       "taste" );
is( $val[1],        "rhubarb" );

ok( $g->delete_edge_attribute_by_id("a", "b", "hot", "color" ) );

ok( !$g->has_edge_attribute_by_id("a", "b", "hot", "color" ) );
ok(  $g->has_edge_attributes_by_id("a", "b", "hot") );
is(  $g->get_edge_attribute_by_id("a", "b", "hot", "taste"),  "rhubarb" );

ok(  $g->delete_edge_attributes_by_id("a", "b", "hot") );
ok( !$g->has_edge_attributes_by_id("a", "b", "hot") );
is(  $g->get_edge_attribute_by_id("a", "b", "hot", "taste"),  undef );

ok( !$g->delete_edge_attribute_by_id("a", "b", "hot", "taste" ) );
ok( !$g->delete_edge_attributes_by_id("a", "b", "hot") );

$attr = $g->get_edge_attributes_by_id("a", "b", "hot");
@name = $g->get_edge_attribute_names_by_id("a", "b", "hot");
@val  = $g->get_edge_attribute_values_by_id("a", "b", "hot");

is( scalar keys %$attr, 0 );
is( scalar @name,       0 );
is( scalar @val,        0 );

is( $g->edges, 0 ); # No "a", "b" anymore.

$g->add_weighted_edge_by_id("c", "d", "hot", 42);

ok( $g->has_edge_by_id("c", "d", "hot") );
is( $g->get_edge_attribute_by_id("c", "d", "hot", "weight"),  42 );

is( $g->edges, 1 );

$g->add_weighted_edges_by_id("c", "d", 43, "e", "f", 44, "hot");
is( $g->get_edge_weight_by_id("c", "d", "hot"),  43 );
is( $g->get_edge_weight_by_id("e", "f", "hot"),  44 );

is( $g->edges, 2 );

$g->add_weighted_path_by_id("c", 45, "d", 46, "e", "hot");
is( $g->get_edge_weight_by_id("c", "d", "hot"),  45 );
is( $g->get_edge_weight_by_id("d", "e", "hot"),  46 );

is( $g->edges, 3 );

use Graph::Undirected;
my $u = Graph::Undirected->new(multiedged => 1);

$u->add_weighted_edge_by_id('a', 'b', 'hot', 123);

is($u->get_edge_weight_by_id('a', 'b', 'hot'), 123);
is($u->get_edge_weight_by_id('b', 'a', 'hot'), 123);

ok($u->set_edge_attributes_by_id('a', 'b', 'hot',
		           { 'color' => 'pearl', 'weight' => 'heavy' }));
$attr = $u->get_edge_attributes_by_id('a', 'b', 'hot');
is(scalar keys %$attr, 2);
is($attr->{color},  'pearl');
is($attr->{weight}, 'heavy');

ok( $g->set_edge_weight_by_id("a", "b", "hot", 42));
is( $g->get_edge_weight_by_id("a", "b", "hot"), 42);
ok( $g->has_edge_weight_by_id("a", "b", "hot"));
ok(!$g->has_edge_weight_by_id("a", "c", "hot"));
ok( $g->delete_edge_weight_by_id("a", "b", "hot"));
ok(!$g->has_edge_weight_by_id("a", "b", "hot"));
is( $g->get_edge_weight_by_id("a", "b", "hot"), undef);

my $h = Graph->new(multiedged => 1);

eval '$h->set_edge_attribute("foo", "bar", "color", "gold")';
like($@, qr/set_edge_attribute: expected non-multiedged/);
