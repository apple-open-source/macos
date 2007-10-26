use t::TestYAML tests => 44; 

local $SIG{__WARN__} = sub { 1 } if $Test::VERSION < 1.20;

ok(YAML::Syck->VERSION);

is(Dump(42),    "--- 42\n");
is(Load("--- 42\n"), 42);

is(Dump(\42),    "--- !!perl/ref \n=: 42\n");
is(${Load("--- !!perl/ref \n=: 42\n")}, 42);

my $x;
$x = \$x;
is(Dump($x),     "--- &1 !!perl/ref \n=: *1\n");
is(Dump(scalar Load(Dump($x))),     "--- &1 !!perl/ref \n=: *1\n");

$YAML::Syck::DumpCode = 0;
is(Dump(sub{ 42 }),  "--- !!perl/code: '{ \"DUMMY\" }'\n");
$YAML::Syck::DumpCode = 1;
ok(Dump(sub{ 42 }) =~ m#--- !!perl/code.*?{.*?42.*?}$#s);

my $like_yaml_pm = 0;
$YAML::Syck::LoadCode = 0;
ok( my $not_sub = Load("--- !!perl/Class '{ \"foo\" . shift }'\n") );

if ( $like_yaml_pm ) {
	is( ref($not_sub), "code" );
	is( eval { $$not_sub }, '{ "foo" . shift }' );
} else {
	is ( $not_sub, '{ "foo" . shift }' );
	ok(1); # stick with the plan
}


$YAML::Syck::LoadCode = 1;
my $sub = Load("--- !!perl/code: '{ \"foo\" . \$_[0] }'\n");

ok( defined $sub );

is( ref($sub), "CODE" );
is( eval { $sub->("bar") }, "foobar" );
is( $@, "", "no error" );

$YAML::Syck::LoadCode = $YAML::Syck::DumpCode = 0;

$YAML::Syck::UseCode = $YAML::Syck::UseCode = 1;

is( eval { Load(Dump(sub { "foo" . shift }))->("bar") }, "foobar" );
is( $@, "", "no error" );
is( eval { Load(Dump(sub { shift() ** 3 }))->(3) }, 27 );

is(Dump(undef), "--- ~\n");
is(Dump('~'), "--- \'~\'\n");
is(Dump('a:'), "--- \"a:\"\n");
is(Dump('a: '), "--- \"a: \"\n");
is(Dump('a '), "--- \"a \"\n");
is(Dump('a: b'), "--- \"a: b\"\n");
is(Dump('a:b'), "--- a:b\n");
is(Load("--- ~\n"), undef);
is(Load("---\n"), undef);
is(Load("--- ''\n"), '');

my $h = {bar => [qw<baz troz>]};
$h->{foo} = $h->{bar};
is(Dump($h), << '.');
--- 
bar: &1 
  - baz
  - troz
foo: *1
.

my $r; $r = \$r;
is(Dump($r), << '.');
--- &1 !!perl/ref 
=: *1
.
is(Dump(scalar Load(Dump($r))), << '.');
--- &1 !!perl/ref 
=: *1
.

# RT #17223
my $y = YAML::Syck::Load("SID:\n type: fixed\n default: ~\n");
eval { $y->{SID}{default} = 'abc' };
is($y->{SID}{default}, 'abc');

is(Load("--- true\n"), "true");
is(Load("--- false\n"), "false");

$YAML::Syck::ImplicitTyping = $YAML::Syck::ImplicitTyping = 1;

is(Load("--- true\n"), 1);
is(Load("--- false\n"), '');

# RT #18752
my $recurse1 = << '.';
--- &1 
Foo: 
  parent: *1
Troz: 
  parent: *1
.

is(Dump(scalar Load($recurse1)), $recurse1, 'recurse 1');

my $recurse2 = << '.';
--- &1 
Bar: 
  parent: *1
Baz: 
  parent: *1
Foo: 
  parent: *1
Troz: 
  parent: *1
Zort: &2 
  Poit: 
    parent: *2
  parent: *1
.

is(Dump(scalar Load($recurse2)), $recurse2, 'recurse 2');

is(Dump(1, 2, 3), "--- 1\n--- 2\n--- 3\n");
is("@{[Load(Dump(1, 2, 3))]}", "1 2 3");

$YAML::Syck::ImplicitBinary = $YAML::Syck::ImplicitBinary = 1;

is(Dump("\xff\xff"), "--- !binary //8=\n");
is(Load("--- !binary //8=\n"), "\xff\xff");
is(Dump("ascii"), "--- ascii\n");

