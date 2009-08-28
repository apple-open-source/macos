#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate);
use Test::More tests => 2;

{
    package Overloaded;

    use overload 'bool' => sub { 0 };

    sub new { bless {} }

    sub foo { 1 }
}

my $ovl = Overloaded->new;

{
    eval
    {
        my @p = ( object => $ovl );
        validate( @p, { object => { isa => 'Overloaded' } } );
    };

    is( $@, q{}, 'overloaded object->isa' );
}

{
    eval
    {
        my @p = ( object => $ovl );
        validate( @p, { object => { can => 'foo' } } );
    };

    is( $@, q{}, 'overloaded object->foo' );
}
