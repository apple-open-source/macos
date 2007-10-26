#!/usr/bin/perl -w

use strict;

use Test::More tests => 10;

use DateTime;

# exercises a bug found in Perl version of _normalize_tai_seconds -
# fixed in 0.15
{
    my $dt = DateTime->new( year => 2000, month => 12 );

    $dt->add( months => 1 )->truncate( to => 'month' )->subtract( seconds => 1 );

    is( $dt->year, 2000, 'year is 2001' );
    is( $dt->month, 12, 'month is 12' );
    is( $dt->hour, 23, 'hour is 23' );
    is( $dt->minute, 59, 'minute is 59' );
    is( $dt->second, 59, 'second is 59' );
}

{
    my $dt = DateTime->new( year => 2000, month => 12 );
    my $dt2 = $dt->clone->add( months => 1 )->subtract( seconds => 1 );

    is( $dt2->year, 2000, 'year is 2001' );
    is( $dt2->month, 12, 'month is 12' );
    is( $dt2->hour, 23, 'hour is 23' );
    is( $dt2->minute, 59, 'minute is 59' );
    is( $dt2->second, 59, 'second is 59' );
}
