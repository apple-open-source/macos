#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate);
use Test::More tests => 13;

{
    my @p = ( foo => 'ClassISA' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'FooBar' } },
                );
    };

    is( $@, q{}, 'no error checking if ClassISA->isa(FooBar)' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'Thingy' } },
                );
    };

    like( $@, qr/was not a 'Thingy'/ );
}

{
    my @p = ( foo => undef );
    eval
    {
        validate( @p,
                  { foo => { isa => 'FooBar' } },
                );
    };

    like( $@, qr/was not a 'FooBar'/ );
}

{
    my @p = ( foo => 'SubClass' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'ClassISA' } },
                );
    };

    is( $@, q{}, 'SubClass->isa(ClassISA)' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'FooBar' } },
                );
    };

    is( $@, q{}, 'SubClass->isa(FooBar)' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'Thingy' } },
                );
    };

    like( $@, qr/was not a 'Thingy'/ );
}

{
    my @p = ( foo => bless {}, 'SubClass' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'ClassISA' } },
                );
    };

    is( $@, q{}, 'SubClass->isa(ClassISA)' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'FooBar' } },
                );
    };

    is( $@, q{}, 'SubClass->isa(FooBar)' );

    eval
    {
        validate( @p,
                  { foo => { isa => 'Thingy' } },
                );
    };

    like( $@, qr/was not a 'Thingy'/ );
}

{
    my @p = ( foo => {} );
    eval
    {
        validate( @p,
                  { foo => { isa => 'Thingy' } },
                );
    };
    like( $@, qr/was not a 'Thingy'/, 'unblessed ref ->isa' );

    @p = ( foo => 27 );
    eval
    {
        validate( @p,
                  { foo => { isa => 'Thingy' } },
                );
    };
    like( $@, qr/was not a 'Thingy'/, 'number isa' );

    @p = ( foo => 'A String' );
    eval
    {
        validate( @p,
                  { foo => { isa => 'Thingy' } },
                );
    };
    like( $@, qr/was not a 'Thingy'/, 'string isa' );

    @p = ( foo => undef );
    eval
    {
        validate( @p,
                  { foo => { isa => 'Thingy' } },
                );
    };
    like( $@, qr/was not a 'Thingy'/, 'undef isa' );
}



package ClassISA;

sub isa
{
    return 1 if $_[1] eq 'FooBar';
    return $_[0]->SUPER::isa($_[1]);
}

sub thingy { 1 }

package SubClass;

use base 'ClassISA';


