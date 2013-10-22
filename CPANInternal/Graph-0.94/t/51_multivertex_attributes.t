use Test::More tests => 69;

use Graph;
my $g = Graph->new(multivertexed => 1);

$g->add_vertex_by_id("a", "hot");

ok( !$g->has_vertex_attributes_by_id("a", "hot") );
ok( !$g->has_vertex_attributes_by_id("a", "hot") );

ok( $g->set_vertex_attribute_by_id("a", "hot", "color", "red") );

ok( $g->has_vertex_attribute_by_id("a", "hot", "color") );
ok( $g->has_vertex_attribute_by_id("a", "hot", "color") );

ok( $g->has_vertex_attributes_by_id("a", "hot") );
ok( $g->has_vertex_attributes_by_id("a", "hot") );

is( $g->get_vertex_attribute_by_id("a", "hot", "color"),  "red" );
is( $g->get_vertex_attribute_by_id("a", "hot", "color"),  "red" );

is( $g->get_vertex_attribute_by_id("a", "hot", "colour"), undef );
is( $g->get_vertex_attribute_by_id("a", "hot", "colour"), undef );

ok( $g->set_vertex_attribute_by_id("a", "hot", "color", "green") );

ok( $g->has_vertex_attributes_by_id("a", "hot") );
ok( $g->has_vertex_attributes_by_id("a", "hot") );

is( $g->get_vertex_attribute_by_id("a", "hot", "color"),  "green" );
is( $g->get_vertex_attribute_by_id("a", "hot", "color"),  "green" );

my $attr = $g->get_vertex_attributes_by_id("a", "hot");
my @name = $g->get_vertex_attribute_names_by_id("a", "hot");
my @val  = $g->get_vertex_attribute_values_by_id("a", "hot");

is( scalar keys %$attr, 1 );
is( scalar @name,       1 );
is( scalar @val,        1 );

is( $attr->{color}, "green" );
is( $name[0],       "color" );
is( $val[0],        "green" );

ok( $g->set_vertex_attribute_by_id("a", "hot", "taste", "rhubarb") );

ok( $g->has_vertex_attributes_by_id("a", "hot") );
ok( $g->has_vertex_attributes_by_id("a", "hot") );

is( $g->get_vertex_attribute_by_id("a", "hot", "taste"),  "rhubarb" );
is( $g->get_vertex_attribute_by_id("a", "hot", "taste"),  "rhubarb" );

is( $g->get_vertex_attribute_by_id("a", "hot", "color"),  "green" );
is( $g->get_vertex_attribute_by_id("a", "hot", "taste"),  "rhubarb" );

$attr = $g->get_vertex_attributes_by_id("a", "hot");
@name = sort $g->get_vertex_attribute_names_by_id("a", "hot");
@val  = sort $g->get_vertex_attribute_values_by_id("a", "hot");

is( scalar keys %$attr, 2 );
is( scalar @name,       2 );
is( scalar @val,        2 );

is( $attr->{color}, "green" );
is( $attr->{taste}, "rhubarb" );
is( $name[0],       "color" );
is( $val[0],        "green" );
is( $name[1],       "taste" );
is( $val[1],        "rhubarb" );

ok( $g->delete_vertex_attribute_by_id("a", "hot", "color" ) );

ok( !$g->has_vertex_attribute_by_id("a", "hot", "color" ) );
ok(  $g->has_vertex_attributes_by_id("a", "hot") );
is(  $g->get_vertex_attribute_by_id("a", "hot", "taste"),  "rhubarb" );

ok(  $g->delete_vertex_attributes_by_id("a", "hot") );
ok( !$g->has_vertex_attributes_by_id("a", "hot") );
is(  $g->get_vertex_attribute_by_id("a", "hot", "taste"),  undef );

ok( !$g->delete_vertex_attribute_by_id("a", "hot", "taste" ) );
ok( !$g->delete_vertex_attributes_by_id("a", "hot") );

$attr = $g->get_vertex_attributes_by_id("a", "hot");
@name = $g->get_vertex_attribute_names_by_id("a", "hot");
@val  = $g->get_vertex_attribute_values_by_id("a", "hot");

is( scalar keys %$attr, 0 );
is( scalar @name,       0 );
is( scalar @val,        0 );

is( $g->vertices, 0 ); # No "a" anymore.

$g->add_weighted_vertex_by_id("b", "cool", 42);

ok( $g->has_vertex_by_id("b", "cool") );
is( $g->get_vertex_weight_by_id("b", "cool"),  42 );

is( $g->vertices, 1 );

$g->add_weighted_vertices_by_id("b", 43, "c", 44, "cool");
is( $g->get_vertex_weight_by_id("b", "cool"),  43 );
is( $g->get_vertex_weight_by_id("c", "cool" ),  44 );

is( $g->vertices, 2 );

ok($g->set_vertex_attributes_by_id('a', 'hot',
		             { 'color' => 'pearl', 'weight' => 'heavy' }));
$attr = $g->get_vertex_attributes_by_id('a', 'hot');
is(scalar keys %$attr, 2);
is($attr->{color},  'pearl');
is($attr->{weight}, 'heavy');

ok( $g->set_vertex_weight_by_id("a", "hot", 42));
is( $g->get_vertex_weight_by_id("a", "hot"), 42);
ok( $g->has_vertex_weight_by_id("a", "hot"));
ok(!$g->has_vertex_weight_by_id("x", "hot"));
ok( $g->delete_vertex_weight_by_id("a", "hot"));
ok(!$g->has_vertex_weight_by_id("a", "hot"));
is( $g->get_vertex_weight_by_id("a", "hot"), undef);

my $h = Graph->new(multivertexed => 1);

eval '$h->set_vertex_attribute("foo", "color", "gold")';
like($@, qr/set_vertex_attribute: expected non-multivertexed/);

