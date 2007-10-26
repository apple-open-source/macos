use Test;
BEGIN { plan test => 13 }

eval
{
    validate_with( params => [ 'foo' ],
                   spec => [ SCALAR ],
                 );
};
ok( !$@ );

eval
{
    validate_with( params => { foo => 5,
                               bar => {} },
                   spec => { foo => SCALAR,
                             bar => HASHREF },
                 );
};
ok( !$@ );

eval
{
    validate_with( params => [],
                   spec => [ SCALAR ],
                   called => 'Yo::Mama',
                 );
};
if ( $ENV{PERL_NO_VALIDATION} )
{
    ok( ! $@ );
}
else
{
    ok( $@ =~ /Yo::Mama/ );
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
    ok( $p{a}, 3 );
    ok( exists $p{b} );
    ok( $p{b}, 'x' );
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

    ok( $p[0], 3 );
    ok( $p[1], 'x' );
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
    main::ok( ! $@ );

    eval
    {
        validate_with( params => [ a => 1, b => 2, c => 3 ],
                       spec => { a => 1, b => 1 },
                       allow_extra => 0,
                     );
    };
    if ( $ENV{PERL_NO_VALIDATION} )
    {
        main::ok( ! $@ );
    }
    else
    {
        main::ok( $@ =~ /was not listed/ );
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
    warn $@ if $@;
    ok( ! $@ );
    ok( $p{foo}, 1 );
}

1;
