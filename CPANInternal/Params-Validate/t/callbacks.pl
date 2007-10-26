use strict;

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

package main;

use Test;

BEGIN { plan test => 3 }

my %allowed = ( foo => 1, baz => 1 );
eval
{
    my @a = ( foo => 'foo' );
    validate( @a, { foo => { callbacks =>
                             { is_allowed => sub { $allowed{ lc $_[0] } } },
                           }
                  } );
};
ok( ! $@ );

eval
{
    my @a = ( foo => 'aksjgakl' );

    validate( @a, { foo => { callbacks =>
                             { is_allowed => sub { $allowed{ lc $_[0] } } },
                           }
                  } );
};
ok( $ENV{PERL_NO_VALIDATION} ? ! $@ :
    $@ =~ /is_allowed/ );

# duplicates code from Lingua::ZH::CCDICT that revelead bug fixed in
# 0.56.
eval
{
    Foo->new( storage => 'InMemory', file => 'something' );
};
ok( ! $@ );



1;
