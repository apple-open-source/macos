#!/usr/bin/perl -w

use strict;

use lib './t';

use Params::Validate qw(validate);

use Test;
plan test => $] == 5.006 ? 2 : 3;

eval { foo() };
ok( $@ =~ /parameter 'foo'/ );

{
    local $Params::Validate::NO_VALIDATION = 1;

    eval { foo() };
    ok( ! $@ );
}

unless ( $] == 5.006 )
{
    eval { foo() };
    ok( $@ =~ /parameter 'foo'/ );
}

sub foo
{
    validate( @_, { foo => 1 } );
}
