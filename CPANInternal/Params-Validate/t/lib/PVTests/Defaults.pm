package PVTests::Defaults;

use strict;
use warnings;

use Params::Validate qw(:all);

use PVTests;
use Test::More;


sub run_tests
{
    plan tests => 21;

    {
        my %def = eval { foo() };

        is( $@, q{},
            'No error calling foo()' );

        is( $def{a}, 1,
            q|Parameter 'a' was not altered| );

        is( $def{b}, 2,
            q|Parameter 'b' was not altered| );

        is( $def{c}, 42,
            q|Correct default assigned for parameter 'c'| );

        is( $def{d}, 0,
            q|Correct default assigned for parameter 'd'| );
    }

    {
        my $def = eval { foo() };

        is( $@, q{},
            'No error calling foo()' );

        is( $def->{a}, 1,
            q|Parameter 'a' was not altered| );

        is( $def->{b}, 2,
            q|Parameter 'b' was not altered| );

        is( $def->{c}, 42,
            q|Correct default assigned for parameter 'c'| );

        is( $def->{d}, 0,
            q|Correct default assigned for parameter 'd'| );
    }

    {
        my @def = eval { bar() };

        is( $@, q{},
            'No error calling bar()' );

        is( $def[0], 1,
            '1st parameter was not altered' );

        is( $def[1], 2,
            '2nd parameter was not altered' );

        is( $def[2], 42,
            'Correct default assigned for 3rd parameter' );

        is( $def[3], 0,
            'Correct default assigned for 4th parameter' );
    }

    {
        my $def = eval { bar() };

        is( $@, q{},
            'No error calling bar()' );

        is( $def->[0], 1,
            '1st parameter was not altered' );

        is( $def->[1], 2,
            '2nd parameter was not altered' );

        is( $def->[2], 42,
            'Correct default assigned for 3rd parameter' );

        is( $def->[3], 0,
            'Correct default assigned for 4th parameter' );
    }

    {
        my $spec = { foobar => { default => [] } };
        my $test1 = validate_with( params => [], spec => $spec );
        $test1->{foobar} = ['x'];

        my $test2 = validate_with( params => [], spec => $spec );
        $test2->{foobar} = ['y'];

        is( $test1->{foobar}[0], 'x',
            'defaults pointing to a reference return a copy of that reference' );
    }
}

sub foo
{
    my @params = ( a => 1, b => 2 );
    return validate( @params, { a => 1,
                                b => { default => 99 },
                                c => { optional => 1, default => 42 },
                                d => { default => 0 },
                              } );
}

sub bar
{
    my @params = ( 1, 2 );

    return validate_pos( @params, 1, { default => 99 }, { default => 42 }, { default => 0 } );
}


1;
