package PVTests::Callbacks;

use strict;
use warnings;

use Params::Validate qw(:all);

use PVTests;
use Test::More;


sub run_tests
{
    plan tests => 3;

    my %allowed = ( foo => 1, baz => 1 );
    eval
    {
        my @a = ( foo => 'foo' );
        validate( @a, { foo => { callbacks =>
                                 { is_allowed => sub { $allowed{ lc $_[0] } } },
                               }
                      } );
    };
    is( $@, q{} );

    eval
    {
        my @a = ( foo => 'aksjgakl' );

        validate( @a, { foo => { callbacks =>
                                 { is_allowed => sub { $allowed{ lc $_[0] } } },
                               }
                      } );
    };

    if ( $ENV{PERL_NO_VALIDATION} )
    {
        is( $@, q{} );
    }
    else
    {
        like( $@, qr/is_allowed/ );
    }

    # duplicates code from Lingua::ZH::CCDICT that revealad bug fixed in
    # 0.56.
    eval
    {
        Foo->new( storage => 'InMemory', file => 'something' );
    };
    is( $@, q{} )
}


package Foo;

use Params::Validate qw(:all);

my %storage = map { lc $_ => $_ } ( qw( InMemory XML BerkeleyDB ) );

sub new
{
    my $class = shift;

    local $^W = 1;

    my %p = validate_with( params => \@_,
                           spec   =>
                           { storage =>
                             { callbacks =>
                               { 'is a valid storage type' =>
                                 sub { $storage{ lc $_[0] } } },
                             },
                           },
                           allow_extra => 1,
                         );

    return 1;
}


1;
