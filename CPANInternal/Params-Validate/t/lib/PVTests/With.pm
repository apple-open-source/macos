package PVTests::With;

use strict;
use warnings;

use Params::Validate qw(:all);

use PVTests;
use Test::More;


sub run_tests
{
    plan tests => 13;

    eval
    {
        validate_with( params => [ 'foo' ],
                       spec => [ SCALAR ],
                     );
    };
    is( $@, q{} );

    eval
    {
        validate_with( params => { foo => 5,
                                   bar => {} },
                       spec => { foo => SCALAR,
                                 bar => HASHREF },
                     );
    };
    is( $@, q{} );

    eval
    {
        validate_with( params => [],
                       spec => [ SCALAR ],
                       called => 'Yo::Mama',
                     );
    };
    if ( $ENV{PERL_NO_VALIDATION} )
    {
        is( $@, q{} );
    }
    else
    {
        like( $@, qr/Yo::Mama/ );
    }

    {
        my %p;
        eval
        {
            %p =
                validate_with( params => [],
                               spec => { a => { default => 3 },
                                         b => { default => 'x' } },
                             );
        };

        ok( exists $p{a} );
        is( $p{a}, 3 );
        ok( exists $p{b} );
        is( $p{b}, 'x' );
    }

    {
        my @p;
        eval
        {
            @p =
                validate_with( params => [],
                               spec => [ { default => 3 },
                                         { default => 'x' } ],
                             );
        };

        is( $p[0], 3 );
        is( $p[1], 'x' );
    }

    {
        package Testing::X;
        use Params::Validate qw(:all);
        validation_options( allow_extra => 1 );

        eval
        {
            validate_with( params => [ a => 1, b => 2, c => 3 ],
                           spec => { a => 1, b => 1 },
                         );
        };
        PVTests::With::is( $@, q{} );

        eval
        {
            validate_with( params => [ a => 1, b => 2, c => 3 ],
                           spec => { a => 1, b => 1 },
                           allow_extra => 0,
                         );
        };
        if ( $ENV{PERL_NO_VALIDATION} )
        {
            PVTests::With::is( $@, q{} );
        }
        else
        {
            PVTests::With::like( $@, qr/was not listed/ );
        }
    }

    {
        # Bug 2791 on rt.cpan.org
        my %p;
        eval
        {
            my @p = { foo => 1 };
            %p = validate_with( params => \@p,
                                spec   => { foo => 1 },
                              );
        };

        is( $@, q{} );
        is( $p{foo}, 1 );
    }

}


1;
