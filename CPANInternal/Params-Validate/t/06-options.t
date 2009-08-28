use strict;
use warnings;

use File::Spec;
use lib File::Spec->catdir( 't', 'lib' );

use PVTests;
use Test::More tests => 4;

use Params::Validate qw(:all);


validation_options( stack_skip => 2 );

sub foo
{
    my %p = validate(@_, { bar => 1 });
}

sub bar { foo(@_) }

sub baz { bar(@_) }

eval { baz() };

like( $@, qr/mandatory.*missing.*call to main::bar/i );

validation_options( stack_skip => 3 );

eval { baz() };
like( $@, qr/mandatory.*missing.*call to main::baz/i );

validation_options
    ( on_fail => sub { die bless { hash => 'ref' }, 'Dead' } );

eval { baz() };

my $e = $@;
is( $e->{hash}, 'ref' );
ok( eval { $e->isa( 'Dead' ); 1; } );



