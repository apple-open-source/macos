use t::TestYAML tests => 18;

ok(YAML::Syck->VERSION);

#use YAML;
#use Test::More 'no_plan';

# This file is based on pyyaml wiki entry for PerlTagScheme, and Ingy's
# guidance.

# http://pyyaml.org/wiki/PerlTagScheme says:
#
# !!perl/hash     # hash reference
# !!perl/array    # array reference
# !!perl/scalar   # scalar reference
# !!perl/code     # code reference
# !!perl/io       # io reference
# !!perl/glob     # a glob (not a ref)
# !!perl/regexp   # a regexp (not a ref)
# !!perl/ref      # a container ref to any of the above
#
# All of the above types can be blessed:
#
# !!perl/hash:Foo::Bar   # hash ref blessed with 'Foo::Bar'
# !!perl/glob:Foo::Bar   # glob blessed with 'Foo::Bar'
#

sub yaml_is {
    my ( $yaml, $expected, @args ) = @_;
    $yaml =~ s/\s+\n/\n/gs;
    @_ = ( $yaml, $expected, @args );
    goto &is;
}

{
	my $hash = { foo => "bar" };
	yaml_is(Dump($hash), "---\nfoo: bar\n");
	bless $hash, "Foo::Bar";
	yaml_is(Dump($hash), "--- !!perl/hash:Foo::Bar\nfoo: bar\n");
}

{
	my $scalar = "foo";
    yaml_is(Dump($scalar), "--- foo\n");
    my $ref = \$scalar;
    yaml_is(Dump($ref), "--- !!perl/ref\n=: foo\n");
	bless $ref, "Foo::Bar";
    yaml_is(Dump($ref), "--- !!perl/scalar:Foo::Bar foo\n");
}

{
	my $hash = { foo => "bar" };
	my $deep_scalar = \$hash;
    yaml_is(Dump($deep_scalar), "--- !!perl/ref\n=:\n  foo: bar\n");
	bless $deep_scalar, "Foo::Bar";
    yaml_is(Dump($deep_scalar), "--- !!perl/ref:Foo::Bar\n=:\n  foo: bar\n");
}

{
	my $array = [ 23, 42 ];
	yaml_is(Dump($array), "---\n- 23\n- 42\n");
	bless $array, "Foo::Bar";
	yaml_is(Dump($array), "--- !!perl/array:Foo::Bar\n- 23\n- 42\n");
}

{
    # FIXME regexes
	my $regex = qr/a(b|c)d/;
    #print Dump($regex);
	bless $regex, "Foo::bar";
    #print Dump($regex);
}

{
	my $hash = Load("--- !!perl/hash\nfoo: bar\n");
	is( ref($hash), "HASH" );
	is( $hash->{foo}, "bar" );
}

{
	my $hash = Load("--- !!perl/hash:Foo::Bar\nfoo: bar\n");
	is( ref($hash), "Foo::Bar" );
	is( $hash->{foo}, "bar" );
}

{
	my $array = Load("--- !!perl/array\n- 42\n- 3\n");
	is( ref($array), "ARRAY" );
	is( $array->[0], 42 );
}

{
	my $array = Load("--- !!perl/array:Foo::Bar\n- 42\n- 3\n");
	is( ref($array), "Foo::Bar" );
	is( $array->[0], 42 );
}

