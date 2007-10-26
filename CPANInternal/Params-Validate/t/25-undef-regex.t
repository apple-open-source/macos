#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate);
use Test::More tests => 2;

{
    my @w;
    local $SIG{__WARN__} = sub { push @w, @_ };

    my @p = ( foo => undef);
    eval { validate( @p, { foo => { regex => qr/^bar/ } } ) };
    ok( $@, 'validation failed' );
    ok( ! @w, 'no warnings' );
}
