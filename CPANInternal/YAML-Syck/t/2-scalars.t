use t::TestYAML tests => 81;

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

$YAML::Syck::LoadCode = 0;
{
    my $not_sub = Load("--- !!perl/code:Some::Class '{ \"foo\" . shift }'\n");
    is( ref $not_sub, "Some::Class" );
    is( $not_sub->("bar"), undef );
}

{
    my $sub = Load("--- !!perl/code '{ \"foo\" . shift }'\n");
    is( ref $sub, "CODE" );
    is( $sub->("bar"), undef );
}

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

# Various edge cases at grok_number boundary
is(Load("--- 42949672\n"), 42949672);
is(Load("--- -42949672\n"), -42949672);
is(Load("--- 429496729\n"), 429496729);
is(Load("--- -429496729\n"), -429496729);
is(Load("--- 4294967296\n"), 4294967296);
is(Load("--- -4294967296\n"), -4294967296);

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

is(Dump("This is Perl 6 User's Golfing System\n", q[--- "This is Perl6 User's Golfing System\n"]));

$YAML::Syck::SingleQuote = $YAML::Syck::SingleQuote = 1;

is(Dump("This is Perl 6 User's Golfing System\n"), qq[--- 'This is Perl 6 User''s Golfing System\n\n'\n]);
is(Dump('042'),    "--- '042'\n");

roundtrip('042');
roundtrip("This\nis\na\ntest");
roundtrip("Newline\n");
roundtrip(" ");
roundtrip("\n");
roundtrip("S p a c e");
roundtrip("Space \n Around");

# If implicit typing is on, quote strings corresponding to implicit boolean and null values
$YAML::Syck::SingleQuote = 0;

is(Dump('N'), "--- 'N'\n");
is(Dump('NO'), "--- 'NO'\n");
is(Dump('No'), "--- 'No'\n");
is(Dump('no'), "--- 'no'\n");
is(Dump('y'), "--- 'y'\n");
is(Dump('YES'), "--- 'YES'\n");
is(Dump('Yes'), "--- 'Yes'\n");
is(Dump('yes'), "--- 'yes'\n");
is(Dump('TRUE'), "--- 'TRUE'\n");
is(Dump('false'), "--- 'false'\n");
is(Dump('off'), "--- 'off'\n");

is(Dump('null'), "--- 'null'\n");
is(Dump('Null'), "--- 'Null'\n");
is(Dump('NULL'), "--- 'NULL'\n");

is(Dump('oN'), "--- oN\n"); # invalid case
is(Dump('oFF'), "--- oFF\n"); # invalid case
is(Dump('nULL'), "--- nULL\n"); # invalid case

