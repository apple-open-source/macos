use t::TestYAML tests => 29, (
    ($] < 5.008) ? (todo => [19..20, 26..29])
                 : ()
);

ok(YAML::Syck->VERSION);

is(Dump(bless({}, 'foo')),    "--- !!perl/hash:foo {}\n\n");

sub ref_ok {
    my $x = Load("--- $_[0] {a: b}\n");
    is(ref($x), $_[1], "ref - $_[0]");
    is($x->{a}, 'b', "data - $_[0]");
}

sub run_ref_ok {
    ref_ok(splice(@_, 0, 2)) while @_;
}

run_ref_ok(qw(
    !!perl/hash:foo     foo
    !perl/foo           foo
    !hs/Foo             hs::Foo
    !haskell.org/Foo    haskell.org::Foo
    !haskell.org/^Foo   haskell.org::Foo
    !!perl              HASH
    !!moose             moose
));

my $rx = qr/123/;
is(Dump($rx), "--- !!perl/regexp (?-xism:123)\n");
is(Dump(Load(Dump($rx))), "--- !!perl/regexp (?-xism:123)\n");

my $rx_obj = bless qr/123/i => 'Foo';
is(Dump($rx_obj), "--- !!perl/regexp:Foo (?i-xsm:123)\n");
is(Dump(Load(Dump($rx_obj))), "--- !!perl/regexp:Foo (?i-xsm:123)\n");

my $obj = bless(\(my $undef) => 'Foo');
is(Dump($obj), "--- !!perl/scalar:Foo ~\n");
is(Dump(Load(Dump($obj))), "--- !!perl/scalar:Foo ~\n");

is(Dump(bless({1..10}, 'foo')),  "--- !!perl/hash:foo \n1: 2\n3: 4\n5: 6\n7: 8\n9: 10\n");

$YAML::Syck::UseCode = 1;

{
	my $hash = Load(Dump(bless({1 .. 4}, "code")));
	is(ref($hash), "code", "blessed to code");
	is(eval { $hash->{1} }, 2, "it's a hash");
}

{
	my $sub = eval { Load(Dump(bless(sub { 42 }, "foobar"))) };
	is(ref($sub), "foobar", "blessed to foobar");
	is(eval { $sub->() }, 42, "it's a CODE");
}

{
	my $sub = eval { Load(Dump(bless(sub { 42 }, "code"))) };
	is(ref($sub), "code", "blessed to code");
	is(eval { $sub->() }, 42, "it's a CODE");
}

exit;
