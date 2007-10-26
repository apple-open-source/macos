#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate);
use Test::More tests => 9;

{
    my @p = ( foo => 'ClassCan' );

    eval
    {
        validate( @p,
                  { foo => { can => 'cancan' } },
                );
    };

    ok( ! $@ );

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

    ok( ! $@ );

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

    ok( ! $@, 'SubClass->can(cancan)' );

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

    ok( ! $@, 'SubClass object->can(cancan)' );

    eval
    {
        validate( @p,
                  { foo => { can => 'thingy' } },
                );
    };

    like( $@, qr/does not have the method: 'thingy'/ );
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


