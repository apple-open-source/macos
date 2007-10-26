#!/usr/bin/perl -w

use strict;

use Test::More tests => 73;

use DateTime;

my @last = ( 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 );
my @leap_last = @last;
$leap_last[1]++;

foreach my $month (1..12)
{
    my $dt = DateTime->last_day_of_month( year  => 2001,
                                          month => $month,
                                          time_zone => 'UTC',
                                        );

    is( $dt->year, 2001, 'check year' );
    is( $dt->month, $month, 'check month' );
    is( $dt->day, $last[ $month - 1 ], 'check day' );
}

foreach my $month (1..12)
{
    my $dt = DateTime->last_day_of_month( year  => 2004,
                                          month => $month,
                                          time_zone => 'UTC',
                                        );

    is( $dt->year, 2004, 'check year' );
    is( $dt->month, $month, 'check month' );
    is( $dt->day, $leap_last[ $month - 1 ], 'check day' );
}

{
    eval { DateTime->last_day_of_month( year => 2000, month => 1, nanosecond => 2000 ) };
    is( $@, '',
        "last_day_of_month should accept nanosecond" );
}
