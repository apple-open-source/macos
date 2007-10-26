use Test;
BEGIN { plan test => 6 }
my $r = '^bar$';

eval
{
    my @a = ( foo => 'bar' );
    validate( @a, { foo => { regex => '^bar$' } } );
};
ok( ! $@ );

eval
{
    my @a = ( foo => 'bar' );
    validate( @a, { foo => { regex => qr/^bar$/ } } );
};
ok( ! $@ );

eval
{
    my @a = ( foo => 'baz' );
    validate( @a, { foo => { regex => '^bar$' } } );
};
ok( $ENV{PERL_NO_VALIDATION} ? ! $@ :
    $@ =~ /'foo'.+did not pass regex check/ );

eval
{
    my @a = ( foo => 'baz' );
    validate( @a, { foo => { regex => qr/^bar$/ } } );
};
ok( $ENV{PERL_NO_VALIDATION} ? ! $@ :
    $@ =~ /'foo'.+did not pass regex check/ );

eval
{
    my @a = ( foo => 'baz', bar => 'quux' );
    validate( @a, { foo => { regex => qr/^baz$/ },
                    bar => { regex => 'uqqx' },
                  } );
};
ok( $ENV{PERL_NO_VALIDATION} ? ! $@ :
    $@ =~ /'bar'.+did not pass regex check/ );

eval
{
    my @a = ( foo => 'baz', bar => 'quux' );
    validate( @a, { foo => { regex => qr/^baz$/ },
                    bar => { regex => qr/^(?:not this|quux)$/ },
                  } );
};
ok( ! $@ );



1;
