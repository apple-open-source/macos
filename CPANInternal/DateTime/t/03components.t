#!/usr/bin/perl -w

use strict;

use Test::More tests => 144;

use DateTime;

my $d = DateTime->new( year => 2001,
                       month => 7,
                       day => 5,
                       hour => 2,
                       minute => 12,
                       second => 50,
                       time_zone => 'UTC',
                     );

is( $d->year, 2001, '->year' );
is( $d->ce_year, 2001, '->ce_year' );
is( $d->month, 7, '->month' );
is( $d->quarter, 3, '->quarter');
is( $d->month_0, 6, '->month_0' );
is( $d->month_name, 'July', '->month_name' );
is( $d->month_abbr, 'Jul', '->month_abbr' );
is( $d->day_of_month, 5, '->day_of_month' );
is( $d->day_of_month_0, 4, '->day_of_month_0' );
is( $d->day, 5, '->day' );
is( $d->day_0, 4, '->day_0' );
is( $d->mday, 5, '->mday' );
is( $d->mday_0, 4, '->mday_0' );
is( $d->mday, 5, '->mday' );
is( $d->mday_0, 4, '->mday_0' );
is( $d->hour, 2, '->hour' );
is( $d->hour_1, 2, '->hour_1' );
is( $d->hour_12, 2, '->hour_12' );
is( $d->hour_12_0, 2, '->hour_12_0' );
is( $d->minute, 12, '->minute' );
is( $d->min, 12, '->min' );
is( $d->second, 50, '->second' );
is( $d->sec, 50, '->sec' );

is( $d->day_of_year, 186, '->day_of_year' );
is( $d->day_of_year_0, 185, '->day_of_year' );
is( $d->day_of_quarter, 5, '->day_of_quarter' );
is( $d->doq, 5, '->doq' );
is( $d->day_of_quarter_0, 4, '->day_of_quarter_0' );
is( $d->doq_0, 4, '->doq_0' );
is( $d->day_of_week, 4, '->day_of_week' );
is( $d->day_of_week_0, 3, '->day_of_week_0' );
is( $d->week_of_month, 1, '->week_of_month' );
is( $d->weekday_of_month, 1, '->weekday_of_month' );
is( $d->wday, 4, '->wday' );
is( $d->wday_0, 3, '->wday_0' );
is( $d->dow, 4, '->dow' );
is( $d->dow_0, 3, '->dow_0' );
is( $d->day_name, 'Thursday', '->day_name' );
is( $d->day_abbr, 'Thu', '->day_abrr' );

is( $d->ymd, '2001-07-05', '->ymd' );
is( $d->ymd('!'), '2001!07!05', "->ymd('!')" );
is( $d->date, '2001-07-05', '->ymd' );

is( $d->mdy, '07-05-2001', '->mdy' );
is( $d->mdy('!'), '07!05!2001', "->mdy('!')" );

is( $d->dmy, '05-07-2001', '->dmy' );
is( $d->dmy('!'), '05!07!2001', "->dmy('!')" );

is( $d->hms, '02:12:50', '->hms' );
is( $d->hms('!'), '02!12!50', "->hms('!')" );
is( $d->time, '02:12:50', '->hms' );

is( $d->datetime, '2001-07-05T02:12:50', '->datetime' );
is( $d->iso8601, '2001-07-05T02:12:50', '->iso8601' );

is( $d->is_leap_year, 0, '->is_leap_year' );

is( $d->era_abbr, 'AD', '->era_abbr' );
is( $d->era, $d->era_abbr, '->era (deprecated)' );
is( $d->era_name, 'Anno Domini', '->era_abbr' );

is( $d->quarter_abbr, 'Q3', '->quarter_abbr' );
is( $d->quarter_name, '3rd quarter', '->quarter_name' );

my $leap_d = DateTime->new( year => 2004,
                            month => 7,
                            day => 5,
                            hour => 2,
                            minute => 12,
                            second => 50,
                            time_zone => 'UTC',
                          );

is( $leap_d->is_leap_year, 1, '->is_leap_year' );

my $sunday = DateTime->new( year   => 2003,
                            month  => 1,
                            day    => 26,
                            time_zone => 'UTC',
                          );

is( $sunday->day_of_week, 7, "Sunday is day 7" );

my $monday = DateTime->new( year   => 2003,
                            month  => 1,
                            day    => 27,
                            time_zone => 'UTC',
                          );

is( $monday->day_of_week, 1, "Monday is day 1" );

{
    # time zone offset should not affect the values returned
    my $d = DateTime->new( year => 2001,
                           month => 7,
                           day => 5,
                           hour => 2,
                           minute => 12,
                           second => 50,
                           time_zone => '-0124',
                         );

    is( $d->year, 2001, '->year' );
    is( $d->ce_year, 2001, '->ce_year' );
    is( $d->month, 7, '->month' );
    is( $d->day_of_month, 5, '->day_of_month' );
    is( $d->hour, 2, '->hour' );
    is( $d->hour_1, 2, '->hour_1' );
    is( $d->minute, 12, '->minute' );
    is( $d->second, 50, '->second' );
}

{
    my $dt0 = DateTime->new( year => 1, time_zone => 'UTC' );

    is( $dt0->year, 1, "year 1 is year 1" );
    is( $dt0->ce_year, 1, "ce_year 1 is year 1" );
    is( $dt0->era_abbr, 'AD', 'era is AD' );
    is( $dt0->year_with_era, '1AD', 'year_with_era is 1AD' );
    is( $dt0->christian_era, 'AD', 'christian_era is AD' );
    is( $dt0->year_with_christian_era, '1AD', 'year_with_christian_era is 1AD' );
    is( $dt0->secular_era, 'CE', 'secular_era is CE' );
    is( $dt0->year_with_secular_era, '1CE', 'year_with_secular_era is 1CE' );

    $dt0->subtract( years => 1 );

    is( $dt0->year, 0, "year 1 minus 1 is year 0" );
    is( $dt0->ce_year, -1, "ce_year 1 minus 1 is year -1" );
    is( $dt0->era_abbr, 'BC', 'era is BC' );
    is( $dt0->year_with_era, '1BC', 'year_with_era is 1BC' );
    is( $dt0->christian_era, 'BC', 'christian_era is BC' );
    is( $dt0->year_with_christian_era, '1BC', 'year_with_christian_era is 1BC' );
    is( $dt0->secular_era, 'BCE', 'secular_era is BCE' );
    is( $dt0->year_with_secular_era, '1BCE', 'year_with_secular_era is 1BCE' );
}

{
    my $dt_neg = DateTime->new( year => -10, time_zone => 'UTC', );
    is( $dt_neg->year, -10, "Year -10 is -10" );
    is( $dt_neg->ce_year, -11, "year -10 is ce_year -11" );

    my $dt1 = $dt_neg + DateTime::Duration->new( years => 10 );
    is( $dt1->year, 0, "year is 0 after adding ten years to year -10" );
    is( $dt1->ce_year, -1, "ce_year is -1 after adding ten years to year -10" );
}

{
    my $dt = DateTime->new( year => 50, month => 2,
                            hour => 3, minute => 20, second => 5,
                            time_zone => 'UTC',
                          );

    is( $dt->ymd('%s'), '0050%s02%s01', 'use %s as separator in ymd' );
    is( $dt->mdy('%s'), '02%s01%s0050', 'use %s as separator in mdy' );
    is( $dt->dmy('%s'), '01%s02%s0050', 'use %s as separator in dmy' );

    is( $dt->hms('%s'), '03%s20%s05', 'use %s as separator in hms' );
}

# test doy in leap year
{
    my $dt = DateTime->new( year => 2000, month => 1, day => 5,
                            time_zone => 'UTC',
                          );

    is( $dt->day_of_year,   5, 'doy for 2000-01-05 should be 5' );
    is( $dt->day_of_year_0, 4, 'doy_0 for 2000-01-05 should be 4' );
}

{
    my $dt = DateTime->new( year => 2000, month => 2, day => 29,
                            time_zone => 'UTC',
                          );

    is( $dt->day_of_year,   60, 'doy for 2000-02-29 should be 60' );
    is( $dt->day_of_year_0, 59, 'doy_0 for 2000-02-29 should be 59' );
}

{
    my $dt = DateTime->new( year => -6, month => 2, day => 25,
                            time_zone => 'UTC',
                          );

    is( $dt->ymd, '-0006-02-25', 'ymd is -0006-02-25' );
    is( $dt->iso8601, '-0006-02-25T00:00:00', 'iso8601 is -0005-02-25T00:00:00' );
    is( $dt->year, -6, 'year is -6' );
    is( $dt->ce_year, -7, 'ce_year is -7' );
}

{
    my $dt = DateTime->new( year => 1996, month => 2, day => 1 );

    is( $dt->quarter, 1, '->quarter is 1' );
    is( $dt->day_of_quarter, 32, '->day_of_quarter' );
}

{
    my $dt = DateTime->new( year => 1996, month => 5, day => 1 );

    is( $dt->quarter, 2, '->quarter is 2' );
    is( $dt->day_of_quarter, 31, '->day_of_quarter' );
}

{
    my $dt = DateTime->new( year => 1996, month => 8, day => 1 );

    is( $dt->quarter, 3, '->quarter is 3' );
    is( $dt->day_of_quarter, 32, '->day_of_quarter' );
}

{
    my $dt = DateTime->new( year => 1996, month => 11, day => 1 );

    is( $dt->quarter, 4, '->quarter is 4' );
    is( $dt->day_of_quarter, 32, '->day_of_quarter' );
}

# nano, micro, and milli seconds
{
    my $dt = DateTime->new( year => 1996, nanosecond => 500_000_000 );

    is( $dt->nanosecond, 500_000_000, 'nanosecond is 500,000,000' );
    is( $dt->microsecond, 500_000, 'microsecond is 500,000' );
    is( $dt->millisecond, 500, 'millisecond is 500' );

    $dt->set( nanosecond => 500_000_500 );

    is( $dt->nanosecond, 500_000_500, 'nanosecond is 500,000,500' );
    is( $dt->microsecond, 500_001, 'microsecond is 500,001' );
    is( $dt->millisecond, 500, 'millisecond is 500' );

    $dt->set( nanosecond => 499_999_999 );

    is( $dt->nanosecond, 499_999_999, 'nanosecond is 499,999,999' );
    is( $dt->microsecond, 500_000, 'microsecond is 500,000' );
    is( $dt->millisecond, 500, 'millisecond is 500' );

    $dt->set( nanosecond => 450_000_001 );

    is( $dt->nanosecond, 450_000_001, 'nanosecond is 450,000,001' );
    is( $dt->microsecond, 450_000, 'microsecond is 450,000' );
    is( $dt->millisecond, 450, 'millisecond is 450' );

    $dt->set( nanosecond => 450_500_000 );

    is( $dt->nanosecond, 450_500_000, 'nanosecond is 450,500,000' );
    is( $dt->microsecond, 450_500, 'microsecond is 450,500' );
    is( $dt->millisecond, 451, 'millisecond is 451' );
}

{
    my $dt = DateTime->new( year => 2003, month => 5, day => 7 );
    is( $dt->weekday_of_month, 1, '->weekday_of_month' );
    is( $dt->week_of_month, 2, '->week_of_month' );
}

{
    my $dt = DateTime->new( year => 2003, month => 5, day => 8 );
    is( $dt->weekday_of_month, 2, '->weekday_of_month' );
    is( $dt->week_of_month, 2, '->week_of_month' );
}

{
    my $dt = DateTime->new( year => 1000, hour => 23 );
    is( $dt->hour,      23, '->hour' );
    is( $dt->hour_1,    23, '->hour_1' );
    is( $dt->hour_12,   11, '->hour_12' );
    is( $dt->hour_12_0, 11, '->hour_12_0' );
}

{
    my $dt = DateTime->new( year => 1000, hour => 0 );
    is( $dt->hour,       0, '->hour' );
    is( $dt->hour_1,    24, '->hour_1' );
    is( $dt->hour_12,   12, '->hour_12' );
    is( $dt->hour_12_0,  0, '->hour_12_0' );
}

SKIP:
{
    skip 'These tests require Test::Warn', 9
        unless eval "use Test::Warn; 1";

    my $dt = DateTime->new( year => 2000 );
    warnings_like( sub { $dt->year(2001) }, qr/is a read-only/,
                   'year() is read-only' );
    warnings_like( sub { $dt->month(5) }, qr/is a read-only/,
                   'month() is read-only' );
    warnings_like( sub { $dt->day(5) }, qr/is a read-only/,
                   'day() is read-only' );
    warnings_like( sub { $dt->hour(5) }, qr/is a read-only/,
                   'hour() is read-only' );
    warnings_like( sub { $dt->minute(5) }, qr/is a read-only/,
                   'minute() is read-only' );
    warnings_like( sub { $dt->second(5) }, qr/is a read-only/,
                   'second() is read-only' );
    warnings_like( sub { $dt->nanosecond(5) }, qr/is a read-only/,
                   'nanosecond() is read-only' );
    warnings_like( sub { $dt->time_zone('America/Chicago') }, qr/is a read-only/,
                   'time_zone() is read-only' );
    warnings_like( sub { $dt->locale('en_US') }, qr/is a read-only/,
                   'locale() is read-only' );
}
