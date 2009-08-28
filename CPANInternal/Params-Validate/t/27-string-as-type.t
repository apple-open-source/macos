#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate);
use Test::More tests => 2;

{
    my @p = ( foo => 1 );

    eval
    {
        validate( @p,
                  { foo => { type => 'SCALAR' } },
                );
    };

    like( $@, qr/\QThe 'foo' parameter ("1") has a type specification which is not a number. It is a string - SCALAR/ );
}

{
    my @p = ( foo => 1 );

    eval
    {
        validate( @p,
                  { foo => { type => undef } },
                );
    };

    like( $@, qr/\QThe 'foo' parameter ("1") has a type specification which is not a number. It is undef/ );

}
