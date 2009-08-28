#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate validate_pos SCALAR);
use Test::More tests => 6;

{
    package Tie::SimpleArray;
    use Tie::Array;
    use base 'Tie::StdArray';
}

{
    package Tie::SimpleHash;
    use Tie::Hash;
    use base 'Tie::StdHash';
}

{
    tie my @p, 'Tie::SimpleArray';

    my %spec = ( foo => 1 );
    push @p, ( foo => 'hello' );

    eval { validate( @p, \%spec ) };
    warn $@ if $@;
    is( $@, q{}, 'validate() call succeeded with tied params array and regular hashref spec' );
}


SKIP:
{
    skip 'Params::Validate segfaults with tied hash for spec', 1;

    my @p;
    tie my %spec, 'Tie::SimpleHash';

    $spec{foo} = 1;
    push @p, ( foo => 'hello' );

    eval { validate( @p, \%spec ) };
    warn $@ if $@;
    is( $@, q{}, 'validate() call succeeded with regular params array and tied hashref spec' );
}

SKIP:
{
    skip 'Params::Validate segfaults with tied hash for spec', 1;

    tie my @p, 'Tie::SimpleArray';
    tie my %spec, 'Tie::SimpleHash';

    $spec{foo} = 1;
    push @p, ( foo => 'hello' );

    eval { validate( @p, \%spec ) };
    warn $@ if $@;
    is( $@, q{}, 'validate() call succeeded with tied params array and tied hashref spec' );
}

{
    tie my @p, 'Tie::SimpleArray';
    my %spec;

    $spec{type} = SCALAR;
    push @p, 'hello';

    eval { validate_pos( @p, \%spec ) };
    warn $@ if $@;
    is( $@, q{}, 'validate_pos() call succeeded with tied params array and regular hashref spec' );
}


SKIP:
{
    skip 'Params::Validate segfaults with tied hash for spec', 1;

    my @p;
    tie my %spec, 'Tie::SimpleHash';

    $spec{type} = SCALAR;
    push @p, 'hello';

    eval { validate_pos( @p, \%spec ) };
    warn $@ if $@;
    is( $@, q{}, 'validate_pos() call succeeded with regular params array and tied hashref spec' );
}

SKIP:
{
    skip 'Params::Validate segfaults with tied hash for spec', 1;

    tie my @p, 'Tie::SimpleArray';
    tie my %spec, 'Tie::SimpleHash';

    $spec{type} = SCALAR;
    push @p, 'hello';

    eval { validate_pos( @p, \%spec ) };
    warn $@ if $@;
    is( $@, q{}, 'validate_pos() call succeeded with tied params array and tied hashref spec' );
}

