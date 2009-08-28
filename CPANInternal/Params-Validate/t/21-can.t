#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate);
use Test::More tests => 13;

{
    my @p = ( foo => 'ClassCan' );

    eval
    {
        validate( @p,
                  { foo => { can => 'cancan' } },
                );
    };

    is( $@, q{} );

    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };

    like( $@, qr/does not have the method: 'thingy'/ );
}

{
    my @p = ( foo => undef );
    eval
    {
        validate( @p,
                  { foo => { can => 'baz' } },
                );
    };

    like( $@, qr/does not have the method: 'baz'/ );
}

{
    my $object = bless {}, 'ClassCan';
    my @p = ( foo => $object );

    eval
    {
        validate( @p,
                  { foo => { can => 'cancan' } },
                );
    };

    is( $@, q{} );

    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };

    like( $@, qr/does not have the method: 'thingy'/ );
}

{
    my @p = ( foo => 'SubClass' );

    eval
    {
        validate( @p,
                  { foo => { can => 'cancan' } },
                );
    };

    is( $@, q{}, 'SubClass->can(cancan)' );

    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };

    like( $@, qr/does not have the method: 'thingy'/ );
}

{
    my $object = bless {}, 'SubClass';
    my @p = ( foo => $object );

    eval
    {
        validate( @p,
                  { foo => { can => 'cancan' } },
                );
    };

    is( $@, q{}, 'SubClass object->can(cancan)' );

    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };

    like( $@, qr/does not have the method: 'thingy'/ );
}

{
    my @p = ( foo => {} );
    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };
    like( $@, qr/does not have the method: 'thingy'/, 'unblessed ref ->can' );

    @p = ( foo => 27 );
    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };
    like( $@, qr/does not have the method: 'thingy'/, 'number can' );

    @p = ( foo => 'A String' );
    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };
    like( $@, qr/does not have the method: 'thingy'/, 'string can' );

    @p = ( foo => undef );
    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };
    like( $@, qr/does not have the method: 'thingy'/, 'undef can' );
}


package ClassCan;

sub can
{
    return 1 if $_[1] eq 'cancan';
    return 0;
}

sub thingy { 1 }

package SubClass;

use base 'ClassCan';


