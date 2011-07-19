#!/usr/bin/perl -w

use strict;

use Test::More tests => 12;

use DateTime;

{
    my $dt = DateTime->new( year => 1900, month => 12, day => 1 );

    is( "$dt", '1900-12-01T00:00:00', 'stringification overloading' );
}

{
    my $dt = DateTime->new( year => 2050, month => 1, day => 15,
                            hour => 20,   minute => 10, second => 10 );

    is( "$dt", '2050-01-15T20:10:10', 'stringification overloading' );

    eval { my $x = $dt + 1 };
    like( $@, qr/Cannot add 1 to a DateTime object/,
          'Cannot add plain scalar to a DateTime object' );

    eval { my $x = $dt + bless {}, 'FooBar' };
    like( $@, qr/Cannot add FooBar=HASH\([^\)]+\) to a DateTime object/,
          'Cannot add plain FooBar object to a DateTime object' );

    eval { my $x = $dt - 1 };
    like( $@, qr/Cannot subtract 1 from a DateTime object/,
          'Cannot subtract plain scalar from a DateTime object' );

    eval { my $x = $dt - bless {}, 'FooBar' };
    like( $@, qr/Cannot subtract FooBar=HASH\([^\)]+\) from a DateTime object/,
          'Cannot subtract plain FooBar object from a DateTime object' );

    eval { my $x = $dt > 1 };
    like( $@, qr/A DateTime object can only be compared to another DateTime object/,
          'Cannot compare a DateTime object to a scalar' );

    eval { my $x = $dt > bless {}, 'FooBar' };
    like( $@, qr/A DateTime object can only be compared to another DateTime object/,
          'Cannot compare a DateTime object to a FooBar object' );

    ok( ! ( $dt eq 'some string' ),
        'DateTime object always compares false to a string' );

    ok( $dt ne 'some string',
        'DateTime object always compares false to a string' );

    ok( $dt eq $dt->clone,
        'DateTime object is equal to a clone of itself' );

    ok( ! ( $dt ne $dt->clone ),
        'DateTime object is equal to a clone of itself (! ne)' );
}
