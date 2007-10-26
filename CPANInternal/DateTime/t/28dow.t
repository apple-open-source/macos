#!/usr/bin/perl -w

use strict;

use Test::More tests => 1112;

use DateTime;

{
    my $dt = DateTime->new( year => 0 );

    is( $dt->year,  0, 'year is 0' );
    is( $dt->month, 1, 'month is 1' );
    is( $dt->day,   1, 'day is 1' );
    is( $dt->day_of_week, 6, 'day of week is 6' );
}

{
    my $dt = DateTime->new( year => 0, month => 12, day => 31 );

    is( $dt->year,   0, 'year is 0' );
    is( $dt->month, 12, 'month is 12' );
    is( $dt->day,   31, 'day is 31' );
    is( $dt->day_of_week, 7, 'day of week is 7' );
}

{
    my $dt = DateTime->new( year => -1 );

    is( $dt->year, -1, 'year is -1' );
    is( $dt->month, 1, 'month is 1' );
    is( $dt->day,   1, 'day is 1' );
    is( $dt->day_of_week, 5, 'day of week is 5' );
}

{
    my $dt = DateTime->new( year => 1 );

    is( $dt->year,   1, 'year is 1' );
    is( $dt->month,  1, 'month is 1' );
    is( $dt->day,    1, 'day is 1' );
    is( $dt->day_of_week, 1, 'day of week is 1' );
}

{
    my $dow = 1;
    for my $year (1, 0, -1)
    {
        my $days_in_year = $year ? 365 : 366;

        for my $doy (reverse 1..$days_in_year)
        {
            is( DateTime->from_day_of_year( year => $year,
                                            day_of_year => $doy,
                                          )->day_of_week,
                $dow,
                "day of week for day $doy of year $year is $dow" );

            $dow--;
            $dow = 7 if $dow == 0;
        }
    }
}
