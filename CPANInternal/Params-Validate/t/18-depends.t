#!/usr/bin/perl -w

use strict;

use Params::Validate qw(validate validate_pos);
use Test::More tests => 19;

{
    my %spec = ( foo => { optional => 1, depends => 'bar' },
                 bar => { optional => 1 },
               );

    my @args = ( bar => 1 );

    eval { validate( @args,\%spec ) };

    is( $@, q{}, "validate() single depends(1): no depends, positive" );

    @args = ( foo => 1, bar => 1 );
    eval { validate( @args,\%spec ) };

    is( $@, q{}, "validate() single depends(2): with depends, positive" );

    @args = ( foo => 1 );
    eval { validate( @args, \%spec ) };

    ok( $@, "validate() single depends(3.a): with depends, negative" );
    like( $@,
          qr(^Parameter 'foo' depends on parameter 'bar', which was not given),
          "validate() single depends(3.b): check error string" );
}

{
    my %spec = ( foo => { optional => 1, depends => [ qw(bar baz) ] },
                 bar => { optional => 1 },
                 baz => { optional => 1 },
               );

    # positive, no depends (single, multiple)
    my @args = ( bar => 1 );
    eval { validate( @args, \%spec ) };
    is( $@, q{}, "validate() multiple depends(1): no depends, single arg, positive" );

    @args = ( bar => 1, baz => 1 );
    eval { validate( @args, \%spec ) };

    is( $@, q{},
	"validate() multiple depends(2): no depends, multiple arg, positive" );

    @args = ( foo => 1, bar => 1, baz => 1 );
    eval { validate( @args, \%spec ) };

    is( $@, q{}, "validate() multiple depends(3): with depends, positive" );

    @args = ( foo => 1, bar => 1 );
    eval { validate( @args, \%spec ) };

    ok( $@, "validate() multiple depends(4.a): with depends, negative, multiple missing" );
    like( $@,
          qr(^Parameter 'foo' depends on parameter 'baz', which was not given),
          "validate() multiple depends (4.b): check error string" );

    @args = ( foo => 1 );
    eval { validate( @args, \%spec ) };

    ok( $@, "validate() multiple depends(5.a): with depends, negative, multiple missing" );
    like( $@,
          qr(^Parameter 'foo' depends on parameter '(bar|baz)', which was not given),
          "validate() multiple depends (5.b): check error string" );
}

{
    # bad depends
    my %spec = ( foo => { optional => 1, depends => { 'bar' => 1 } },
                 bar => { optional => 1 },
               );

    my @args = ( foo => 1 );
    eval { validate( @args, \%spec ) };

    ok( $@, "validate() bad depends spec (1.a): depends is a hashref" );
    like( $@,
          qr(^Arguments to 'depends' must be a scalar or arrayref),
          "validate() bad depends spec (1.a): check error string" );
}

{
    my @spec = ( { optional => 1 } );

    my @args = qw(1);
    eval { validate_pos( @args, @spec ) };

    is( $@, q{}, "validate_pos() no depends, positive" );
}

{
    my @spec = ( { optional => 1, depends => 2 }, {optional => 1} );

    my @args = qw(1 1);
    eval { validate_pos( @args, @spec ) };

    is( $@, q{}, "validate_pos() single depends (1): with depends, positive" );
}

{
    my @spec = ( { optional => 1, depends => 4 },
                 { optional => 1 }, { optional => 1 },
                 { optional => 1 } );

    my @args = qw(1 0);
    eval { validate_pos( @args, @spec ) };

    ok( $@, "validate_pos() single depends (2.a): with depends, negative" );
    like( $@,
          qr(^Parameter #1 depends on parameter #4, which was not given),
          "validate_pos() single depends (2.b): check error" );
}

{
    my @spec = ( { optional => 1, depends => [ 2, 3 ] },
                 { optional => 1 },
                 0
               );
    my @args = qw(1);
    eval { validate_pos( @args, @spec ) };

    ok( $@, "validate_pos() multiple depends (1.a): with depends, bad args negative" );
    like( $@,
          qr{^Arguments to 'depends' for validate_pos\(\) must be a scalar},
          "validate_pos() multiple depends (1.b): check error" );
}
