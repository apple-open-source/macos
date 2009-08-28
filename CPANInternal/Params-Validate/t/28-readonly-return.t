#!/usr/bin/perl -w

use strict;
use Test::More;

BEGIN
{
    eval 'use File::Temp';
    if ($@)
    {
        plan skip_all => 'Need File::Temp for this test';
    }
    else
    {
        plan tests => 9;
    }
}

use Devel::Peek qw( SvREFCNT );
use File::Temp qw( tempfile );
use Params::Validate qw( validate SCALAR HANDLE );

{
    my $fh = tempfile();
    my @p = ( foo => 1,
              bar => $fh,
            );

    my $ref = val1(@p);

    eval { $ref->{foo} = 2 };
    ok( ! $@, 'returned hashref values are not read only' );
    is( $ref->{foo}, 2, 'double check that setting value worked' );
    is( $fh, $ref->{bar}, 'filehandle is not copied during validation' );
}

{
    package ScopeTest;

    my $live = 0;

    sub new { $live++; bless {}, shift }
    sub DESTROY { $live-- }

    sub Live { $live }
}

{
    my @p = ( foo => ScopeTest->new() );

    is( ScopeTest->Live(), 1,
        'one live object' );

    my $ref = val2(@p);

    isa_ok( $ref->{foo}, 'ScopeTest' );

    @p = ();

    is( ScopeTest->Live(), 1,
        'still one live object' );

    ok( defined $ref->{foo},
        'foo key stays in scope after original version goes out of scope' );
    is( SvREFCNT( $ref->{foo} ), 1,
        'ref count for reference is 1' );

    undef $ref->{foo};

    is( ScopeTest->Live(), 0,
        'no live objects' );
}

sub val1
{
    my $ref = validate( @_,
                        { foo => { type => SCALAR },
                          bar => { type => HANDLE, optional => 1 },
                        },
                      );

    return $ref;
}

sub val2
{
    my $ref = validate( @_,
                        { foo => 1,
                        },
                      );

    return $ref;
}
