#!/usr/bin/perl -Tw

use strict;

use Params::Validate qw(validate validate_pos);
use Test::More;

eval "use Test::Taint 0.02";
if ($@)
{
    plan skip_all => 'These tests require Test::Taint 0.02+ to run';
}
else
{
    plan tests => 9;
}

taint_checking_ok( 'These tests are meaningless unless we are in taint mode.' );

{
    my $value = 7;
    taint($value);

    tainted_ok( $value, 'make sure $value is tainted' );

    my @p = ( value => $value );
    my %p = validate( @p, { value => { regex   => qr/^\d+$/,
                                       untaint => 1,
                                     },
                          },
                    );

    untainted_ok( $p{value}, 'value is untainted after validation' );
}

{
    my $value = 'foo';

    taint($value);

    tainted_ok( $value, 'make sure $value is tainted' );

    my @p = ( $value );
    my ($new_value) = validate_pos( @p, { regex   => qr/foo/,
                                          untaint => 1,
                                        },
                                  );

    untainted_ok( $new_value, 'value is untainted after validation' );
}

{
    my $value = 7;
    taint($value);

    tainted_ok( $value, 'make sure $value is tainted' );

    my @p = ( value => $value );
    my %p = validate( @p, { value => { regex   => qr/^\d+$/,
                                     },
                          },
                    );

    tainted_ok( $p{value}, 'value is still tainted after validation' );
}

{
    my $value = 'foo';

    taint($value);

    tainted_ok( $value, 'make sure $value is tainted' );

    my @p = ( $value );
    my ($new_value) = validate_pos( @p, { regex   => qr/foo/,
                                        },
                                  );

    tainted_ok( $new_value, 'value is still tainted after validation' );
}
