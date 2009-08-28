use strict;
use warnings;

use File::Spec;
use lib File::Spec->catdir( 't', 'lib' );

use PVTests;
use Test::More tests => 10;

use Attribute::Params::Validate;
use Params::Validate qw(:all);


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

sub buz : ValidatePos( 1 )
{
    return $_[0];
}

sub quux :ValidatePos( { type => SCALAR }, 1 )
{
    return $_[0];
}

my $res = eval { foo( c => 1 ) };
is( $@, q{},
    "Call foo with a scalar" );

is( $res, 1,
    'Check return value from foo( c => 1 )' );

eval { foo( c => [] ) };

like( $@, qr/The 'c' parameter .* was an 'arrayref'/,
      'Check exception thrown from foo( c => [] )' );

$res = eval { main->bar( c => 1 ) };
is( $@, q{},
    'Call bar with a scalar' );

is( $res, 1,
    'Check return value from bar( c => 1 )' );

eval { baz( foo => [1,2,3,4] ) };

like( $@, qr/The 'foo' parameter .* did not pass the '5 elements' callback/,
      'Check exception thrown from baz( foo => [1,2,3,4] )' );

$res = eval { baz( foo => [5,4,3,2,1] ) };

is( $@, q{},
    'Call baz( foo => [5,4,3,2,1] )' );

is( $res, 5,
    'Check return value from baz( foo => [5,4,3,2,1] )' );

eval { buz( [], 1 ) };

like( $@, qr/2 parameters were passed to .* but 1 was expected/,
      'Check exception thrown from quux( [], 1 )' );

$res = eval { quux( 1, [] ) };

is( $@, q{},
    'Call quux' );
