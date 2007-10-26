#!/usr/bin/perl -w

use strict;

BEGIN
{
    if ($] < 5.006)
    {
	print "1..0\n";
	exit;
    }

    eval "use Attribute::Handlers";
    if ($@)
    {
	print "1..0\n";
	exit;
    }

    $ENV{PERL_NO_VALIDATION} = 0;
    require Attribute::Params::Validate;
    Params::Validate->import(':all');
}

if ( $] == 5.006 )
{
    warn <<'EOF';

Skipping tests for Perl 5.6.0.  5.6.0 core dumps all over during the
tests.  This may just have to do with the test code rather than the
module itself.  5.6.1 works fine when I tested it.  5.6.0 is buggy.
You are encouraged to upgrade.
EOF

    print "1..0\n";
    exit;
}

print "1..13\n";

sub foo :Validate( c => { type => SCALAR } )
{
    my %data = @_;
    return $data{c};
}

sub bar :Validate( c => { type => SCALAR } ) method
{
    my $self = shift;
    my %data = @_;
    return $data{c};
}

sub baz :Validate( foo => { type => ARRAYREF, callbacks => { '5 elements' => sub { @{shift()} == 5 } } } )
{
    my %data = @_;
    return $data{foo}->[0];
}

sub quux :ValidatePos( { type => SCALAR }, 1 )
{
    return $_[0];
}

my $res = eval { foo( c => 1 ) };
ok( ! $@,
    "Calling foo with a scalar failed: $@\n" );

ok( $res == 1,
    "Return value from foo( c => 1 ) was not 1, it was $res\n" );

eval { foo( c => [] ) };

ok( $@,
    "No exception was thrown when calling foo( c => [] )\n" );

ok( $@ =~ /The 'a' parameter to .* was an 'arrayref'/,
    "The exception thrown when calling foo( c => [] ) was $@\n" );

$res = eval { main->bar( c => 1 ) };
ok( ! $@,
    "Calling bar with a scalar failed: $@\n" );

ok( $res == 1,
    "Return value from bar( c => 1 ) was not 1, it was $res\n" );

eval { baz( foo => [1,2,3,4] ) };

ok( $@,
    "No exception was thrown when calling baz( foo => [1,2,3,4] )\n" );

ok( $@ =~ /The 'foo' parameter to .* did not pass the '5 elements' callback/,
    "The exception thrown when calling baz( foo => [1,2,3,4] ) was $@\n" );

$res = eval { baz( foo => [5,4,3,2,1] ) };

ok( ! $@,
    "Calling baz( foo => [5,4,3,2,1] ) threw an exception: $@\n" );

ok( $res == 5,
    "The return value from baz( foo => [5,4,3,2,1] ) was $res\n" );

eval { quux( [], 1 ) };

ok( $@,
    "No exception was thrown when calling quux( [], 1 )\n" );

ok( $@ =~ /2 parameters were passed to .* but 1 was expected/,
    "The exception thrown when calling quux( [], 1 ) was $@\n" );

$res = eval { quux( 1, [] ) };

ok( ! $@,
    "Calling quux failed: $@\n" );

sub ok
{
    my $ok = !!shift;
    use vars qw($TESTNUM);
    $TESTNUM++;
    print "not "x!$ok, "ok $TESTNUM\n";
    print "@_\n" if !$ok;
}

