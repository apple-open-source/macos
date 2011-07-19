#!/usr/bin/perl -w

use strict;

use Test::More tests => 172;
use DateTime;

# tests using UTC times
{
    # 1972-06-30T23:58:20 UTC
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 23, minute => 58, second => 20,
                           time_zone => 'UTC',
                         );
    my $t1 = $t->clone;

    is( $t->year, 1972, "year is 1972" );
    is( $t->minute, 58, "minute is 58" );
    is( $t->second, 20, "second is 20" );

    # 1972-06-30T23:59:20 UTC
    $t->add( seconds => 60 );
    is( $t->year, 1972, "year is 1972" );
    is( $t->minute, 59, "minute is 59" );
    is( $t->second, 20, "second is 20" );

    # 1972-07-01T00:00:19 UTC
    $t->add( seconds => 60 );
    is( $t->year, 1972, "year is 1972" );
    is( $t->minute, 0, "minute is 0" );
    is( $t->second, 19, "second is 19" );

    # 1972-06-30T23:59:60 UTC
    $t->subtract( seconds => 20 );
    is( $t->year, 1972, "year is 1972" );
    is( $t->minute, 59, "minute is 59" );
    is( $t->second, 60, "second is 60" );
    is( $t->{utc_rd_secs} , 86400, "utc_rd_secs is 86400" );


    # subtract_datetime
    my $t2 = DateTime->new( year => 1972, month => 07, day => 1,
                            hour => 0, minute => 0, second => 20,
                            time_zone => 'UTC',
                          );
    my $dur = $t2->subtract_datetime_absolute($t1);
    is( $dur->delta_seconds, 121, "delta_seconds is 121" );

    $dur = $t1->subtract_datetime_absolute($t2);
    is( $dur->delta_seconds, -121, "delta_seconds is -121" );
}

{
    # tests using floating times
    # a floating time has no leap seconds

    my $t = DateTime->new( year => 1971, month => 12, day => 31,
                           hour => 23, minute => 58, second => 20,
                           time_zone => 'floating',
                         );
    my $t1 = $t->clone;

    $t->add( seconds => 60);
    is( $t->minute, 59, "min" );
    is( $t->second, 20, "sec" );

    $t->add( seconds => 60);
    is( $t->minute, 0, "min" );
    is( $t->second, 20, "sec" );

    # subtract_datetime, using floating times

    my $t2 = DateTime->new( year => 1972, month => 1, day => 1,
                            hour => 0, minute => 0, second => 20,
                            time_zone => 'floating',
                          );
    my $dur = $t2->subtract_datetime_absolute($t1);
    is( $dur->delta_seconds, 120, "delta_seconds is 120" );

    $dur = $t1->subtract_datetime_absolute($t2);
    is( $dur->delta_seconds, -120, "delta_seconds is -120" );
}

{
    # tests using time zones
    # leap seconds occur during _UTC_ midnight

    # 1972-06-30 20:58:20 -03:00 = 1972-06-30 23:58:20 UTC
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 20, minute => 58, second => 20,
                           time_zone => 'America/Sao_Paulo',
                         );

    $t->add( seconds => 60 );
    is( $t->datetime, '1972-06-30T20:59:20', "normal add" );
    is( $t->minute, 59, "min" );
    is( $t->second, 20, "sec" );

    $t->add( seconds => 60 );
    is( $t->datetime, '1972-06-30T21:00:19', "add over a leap second" );
    is( $t->minute, 0, "min" );
    is( $t->second, 19, "sec" );

    $t->subtract( seconds => 20 );
    is( $t->datetime, '1972-06-30T20:59:60', "subtract over a leap second" );
    is( $t->minute, 59, "min" );
    is( $t->second, 60, "sec" );
    is( $t->{utc_rd_secs} , 86400, "rd_sec" );
}

# test that we can set second to 60 (negative offset)
{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 20, minute => 59, second => 60,
                           time_zone => 'America/Sao_Paulo',
                         );

    is( $t->second, 60, 'second set to 60 in constructor' );
}

{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 21, minute => 0, second => 0,
                           time_zone => 'America/Sao_Paulo',
                         );

    is( $t->second, 0, 'datetime just after leap second' );
}

{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 21, minute => 0, second => 1,
                           time_zone => 'America/Sao_Paulo',
                         );

    is( $t->second, 1, 'datetime two seconds after leap second' );
}

# test that we can set second to 60 (negative offset)
{
    eval
    {
        my $t = DateTime->new( year => 1972, month => 6, day => 30,
                               hour => 22, minute => 59, second => 60,
                               time_zone => '-0100',
                             );

        is( $t->second, 60, 'second set to 60 in constructor, negative TZ offset' );
    };

    if ($@)
    {
        ok( 0, "Error setting second to 60 in constructor: $@" );
    }
}

# test that we can set second to 60 (positive offset)
{
    eval
    {
        my $t = DateTime->new( year => 1972, month => 7, day => 1,
                               hour => 0, minute => 59, second => 60,
                               time_zone => '+0100',
                             );

        is( $t->second, 60, 'second set to 60 in constructor, positive TZ offset' );
    };

    if ($@)
    {
        ok( 0, "Error setting second to 60 in constructor, positive TZ offset: $@" );
    }
}

{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 0, minute => 59, second => 59,
                           time_zone => '+0100',
                         );

    is( $t->second, 59, 'datetime just before leap second' );
}

{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 1, minute => 0, second => 0,
                           time_zone => '+0100',
                         );

    is( $t->second, 0, 'datetime just after leap second' );
}


{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 1, minute => 0, second => 1,
                           time_zone => '+0100',
                         );

    is( $t->second, 1, 'datetime two seconds after leap second' );
}

{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 0, minute => 0, second => 29,
                           time_zone => '+00:00:30',
                         );

    is( $t->second, 29, 'time zone +00:00:30 and leap seconds, second value' );
    is( $t->minute,  0, 'time zone +00:00:30 and leap seconds, minute value' );
}

{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 20, minute => 59, second => 60,
                           time_zone => 'America/Sao_Paulo',
                         );

    $t->set_time_zone( 'UTC' );
    is( $t->second, 60, 'second after setting time zone' );
    is( $t->hour, 23, 'hour after setting time zone' );

    $t->add( days => 1 );
    is( $t->datetime, '1972-07-02T00:00:00',
        'add 1 day starting on leap second' );

    $t->subtract( days => 1 );

    is( $t->datetime, '1972-07-01T00:00:00',
        'add and subtract 1 day starting on leap second' );

    is( $t->leap_seconds, 1, 'datetime has 1 leap second' );
}

{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 23, minute => 59, second => 59,
                           time_zone => 'UTC',
                         );

    is( $t->epoch, 78796799, 'epoch just before first leap second is 78796799' );

    $t->add( seconds => 1 );

    is( $t->epoch, 78796800, 'epoch of first leap second is 78796800' );

    $t->add( seconds => 1 );

    is( $t->epoch, 78796800, 'epoch of first second after first leap second is 78796700' );
}

{
    my $dt = DateTime->new( year => 2003, time_zone => 'UTC' );

    is( $dt->leap_seconds, 22, 'datetime has 22 leap seconds' );
}

{
    my $dt = DateTime->new( year => 2003, time_zone => 'floating' );

    is( $dt->leap_seconds, 0, 'floating datetime has 0 leap seconds' );
}

# date math across leap seconds distinguishes between minutes and second
{
    my $t = DateTime->new( year => 1972, month => 12, day => 31,
                           hour => 23, minute => 59, second => 30,
                           time_zone => 'UTC' );

    $t->add( minutes => 1 );

    is( $t->year, 1973, '+1 minute, year == 1973' );
    is( $t->month, 1, '+1 minute, month == 1' );
    is( $t->day, 1, '+1 minute, day == 1' );
    is( $t->hour, 0, '+1 minute, hour == 0' );
    is( $t->minute, 0, '+1 minute, minute == 0' );
    is( $t->second, 30, '+1 minute, second == 30' );
}

{
    my $t = DateTime->new( year => 1972, month => 12, day => 31,
                           hour => 23, minute => 59, second => 30,
                           time_zone => 'UTC' );

    $t->add( seconds => 60 );

    is( $t->year, 1973, '+60 seconds, year == 1973' );
    is( $t->month, 1, '+60 seconds, month == 1' );
    is( $t->day, 1, '+60 seconds, day == 1' );
    is( $t->hour, 0, '+60 seconds, hour == 0' );
    is( $t->minute, 0, '+60 seconds, minute == 0' );
    is( $t->second, 29, '+60 seconds, second == 29' );
}

{
    my $t = DateTime->new( year => 1972, month => 12, day => 31,
                           hour => 23, minute => 59, second => 30,
                           time_zone => 'UTC' );

    $t->add( minutes => 1, seconds => 1 );

    is( $t->year, 1973, '+1 minute & 1 second, year == 1973' );
    is( $t->month, 1, '+1 minute & 1 second, month == 1' );
    is( $t->day, 1, '+1 minute & 1 second, day == 1' );
    is( $t->hour, 0, '+1 minute & 1 second, hour == 0' );
    is( $t->minute, 0, '+1 minute & 1 second, minute == 0' );
    is( $t->second, 31, '+1 minute & 1 second, second == 31' );
}

{
    eval { DateTime->new( year => 1972, month => 12, day => 31,
                          hour => 23, minute => 59, second => 61,
                          time_zone => 'UTC',
                        ) };
    ok( $@, "Cannot give second of 61 except when it matches a leap second" );

    eval { DateTime->new( year => 1972, month => 12, day => 31,
                          hour => 23, minute => 58, second => 60,
                          time_zone => 'UTC',
                        ) };
    ok( $@, "Cannot give second of 60 except when it matches a leap second" );

    eval { DateTime->new( year => 1972, month => 12, day => 31,
                          hour => 23, minute => 59, second => 60,
                          time_zone => 'floating',
                        ) };
    ok( $@, "Cannot give second of 60 with floating time zone" );
}


{
    my $dt1 = DateTime->new( year => 1998, month => 12, day => 31,
                             hour => 23,  minute => 59, second => 60,
                             time_zone => 'UTC',
                           );

    my $dt2 = DateTime->new( year => 1998, month => 12, day => 31,
                             hour => 23,  minute => 58, second => 50,
                             time_zone => 'UTC',
                           );

    my $pos_dur = $dt1 - $dt2;

    is( $pos_dur->delta_minutes, 1, 'delta_minutes is 1' );
    is( $pos_dur->delta_seconds, 10, 'delta_seconds is 10' );

    my $neg_dur = $dt2 - $dt1;

    is( $neg_dur->delta_minutes, -1, 'delta_minutes is -1' );
    is( $neg_dur->delta_seconds, -10, 'delta_seconds is -10' );
}


{
    my $dt1 = DateTime->new( year => 1998, month => 12, day => 31,
                             hour => 23,  minute => 59, second => 55,
                             time_zone => 'UTC',
                           );

    my $dt2 = DateTime->new( year => 1998, month => 12, day => 31,
                             hour => 23,  minute => 58, second => 50,
                             time_zone => 'UTC',
                           );

    my $pos_dur = $dt1 - $dt2;

    is( $pos_dur->delta_minutes, 1, 'delta_minutes is 1' );
    is( $pos_dur->delta_seconds, 5, 'delta_seconds is 5' );

    my $neg_dur = $dt2 - $dt1;

    is( $neg_dur->delta_minutes, -1, 'delta_minutes is -1' );
    is( $neg_dur->delta_seconds, -5, 'delta_seconds is -5' );
}

{
    my $dt1 = DateTime->new( year => 1998, month => 12, day => 31,
                             hour => 23,  minute => 59, second => 55,
                             time_zone => 'UTC',
                           );

    my $dt2 = DateTime->new( year => 1999, month => 1, day => 1,
                             hour => 0,  minute => 0, second => 30,
                             time_zone => 'UTC',
                           );

    my $pos_dur = $dt2 - $dt1;

    is( $pos_dur->delta_minutes, 0, 'delta_minutes is 0' );
    is( $pos_dur->delta_seconds, 36, 'delta_seconds is 36' );

    my $neg_dur = $dt1 - $dt2;

    is( $neg_dur->delta_minutes, 0, 'delta_minutes is 0' );
    is( $neg_dur->delta_seconds, -36, 'delta_seconds is -36' );
}

# catch off-by-one when carrying a leap second
{
    my $dt1 = DateTime->new( year => 1998, month => 12, day => 31,
                             hour => 23,  minute => 59, second => 0,
                             nanosecond => 1,
                             time_zone => 'UTC',
                           );

    my $dt2 = DateTime->new( year => 1999, month => 1, day => 1,
                             hour => 0,  minute => 0, second => 0,
                             time_zone => 'UTC',
                           );

    my $pos_dur = $dt2 - $dt1;

    is( $pos_dur->delta_minutes, 0, 'delta_minutes is 0' );
    is( $pos_dur->delta_seconds, 60, 'delta_seconds is 60' );
    is( $pos_dur->delta_nanoseconds, 999999999, 'delta_nanoseconds is 999...' );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->add( days => 2 );

    is( $dt->datetime, '1972-07-02T23:58:20', "add two days crossing a leap second (UTC)" );
}

# a bunch of tests that math works across a leap second for various time zones
{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->add( days => 2 );

    is( $dt->datetime, '1972-07-02T20:58:20', "add two days crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 1,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->add( days => 2 );

    is( $dt->datetime, '1972-07-03T02:58:20', "add two days crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->add( hours => 48 );

    is( $dt->datetime, '1972-07-02T23:58:20', "add 48 hours crossing a leap second (UTC)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->add( hours => 48 );

    is( $dt->datetime, '1972-07-02T20:58:20', "add 48 hours crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 1,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->add( hours => 48 );

    is( $dt->datetime, '1972-07-03T02:58:20', "add 48 hours crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->add( minutes => 2880 );

    is( $dt->datetime, '1972-07-02T23:58:20', "add 2880 minutes crossing a leap second (UTC)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->add( minutes => 2880 );

    is( $dt->datetime, '1972-07-02T20:58:20', "add 2880 minutes crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 1,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->add( minutes => 2880 );

    is( $dt->datetime, '1972-07-03T02:58:20', "add 2880 minutes crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->add( seconds => 172801 );

    is( $dt->datetime, '1972-07-02T23:58:20', "add 172801 seconds crossing a leap second (UTC)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->add( seconds => 172801 );

    is( $dt->datetime, '1972-07-02T20:58:20', "add 172801 seconds crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 1,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->add( seconds => 172801 );

    is( $dt->datetime, '1972-07-03T02:58:20', "add 172801 seconds crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->subtract( days => 2 );

    is( $dt->datetime, '1972-06-30T23:58:20', "subtract two days crossing a leap second (UTC)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->subtract( days => 2 );

    is( $dt->datetime, '1972-06-30T20:58:20', "subtract two days crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 3,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->subtract( days => 2 );

    is( $dt->datetime, '1972-07-01T02:58:20', "subtract two days crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->subtract( hours => 48 );

    is( $dt->datetime, '1972-06-30T23:58:20', "subtract 48 hours crossing a leap second (UTC)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->subtract( hours => 48 );

    is( $dt->datetime, '1972-06-30T20:58:20', "subtract 48 hours crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 3,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->subtract( hours => 48 );

    is( $dt->datetime, '1972-07-01T02:58:20', "subtract 48 hours crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->subtract( minutes => 2880 );

    is( $dt->datetime, '1972-06-30T23:58:20', "subtract 2880 minutes crossing a leap second (UTC)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->subtract( minutes => 2880 );

    is( $dt->datetime, '1972-06-30T20:58:20', "subtract 2880 minutes crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 3,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->subtract( minutes => 2880 );

    is( $dt->datetime, '1972-07-01T02:58:20', "subtract 2880 minutes crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 23, minute => 58, second => 20,
                            time_zone => 'UTC',
                          );

    $dt->subtract( seconds => 172801 );

    is( $dt->datetime, '1972-06-30T23:58:20', "subtract 172801 seconds crossing a leap second (UTC)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 2,
                            hour => 20, minute => 58, second => 20,
                            time_zone => '-0300',
                          );

    $dt->subtract( seconds => 172801 );

    is( $dt->datetime, '1972-06-30T20:58:20', "subtract 172801 seconds crossing a leap second (-0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 3,
                            hour => 2, minute => 58, second => 20,
                            time_zone => '+0300',
                          );

    $dt->subtract( seconds => 172801 );

    is( $dt->datetime, '1972-07-01T02:58:20', "subtract 172801 seconds crossing a leap second (+0300)" );
}

{
    my $dt = DateTime->new( year => 1972, month => 7, day => 1,
                            hour => 12, minute => 58, second => 20,
                            time_zone => '+1200',
                          );

    $dt->set_time_zone( '-1200' );

    is( $dt->datetime, '1972-06-30T12:58:20', "24 hour time zone change near leap second" );
}

{
    my $dt = DateTime->new( year => 1972, month => 6, day => 30,
                            hour => 12, minute => 58, second => 20,
                            time_zone => '-1200',
                          );

    $dt->set_time_zone( '+1200' );

    is( $dt->datetime, '1972-07-01T12:58:20', "24 hour time zone change near leap second" );
}

{
    my $dt = DateTime->new(year => 1997, month => 7, day => 1,
                           hour => 0, minute => 59, second => 59,
                           time_zone => '+0100');

    is( $dt->datetime, '1997-07-01T00:59:59', '+0100 time leap second T-1' );

    $dt->set_time_zone('UTC');

    is( $dt->datetime, '1997-06-30T23:59:59', 'UTC time leap second T-1' );
}

{
    my $dt = DateTime->new(year => 1997, month => 7, day => 1,
                           hour => 0, minute => 59, second => 60,
                           time_zone => '+0100');

    is( $dt->datetime, '1997-07-01T00:59:60', 'local time leap second T-0' );

    $dt->set_time_zone('UTC');

    is( $dt->datetime, '1997-06-30T23:59:60', 'UTC time leap second T-0' );
}

{
    my $dt = DateTime->new(year => 1997, month => 7, day => 1,
                           hour => 1, minute => 0, second => 0,
                           time_zone => '+0100');

    is( $dt->datetime, '1997-07-01T01:00:00', 'local time leap second T+1' );

    $dt->set_time_zone('UTC');

    is( $dt->datetime, '1997-07-01T00:00:00', 'UTC time leap second T+1' );
}

{
    my $dt = DateTime->new(year => 1997, month => 7, day => 1,
                           hour => 23, minute => 59, second => 59,
                           time_zone => '+0100');

    is( $dt->datetime, '1997-07-01T23:59:59', 'local time end of leap second day' );

    $dt->set_time_zone('UTC');

    is( $dt->datetime, '1997-07-01T22:59:59', 'UTC time end of leap second day' );
}


{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 22, minute => 59, second => 59,
                           time_zone => '-0100');

    is( $dt->datetime, '1997-06-30T22:59:59', '-0100 time leap second T-1' );

    $dt->set_time_zone('UTC');

    is( $dt->datetime, '1997-06-30T23:59:59', 'UTC time leap second T-1' );
}

{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 22, minute => 59, second => 60,
                           time_zone => '-0100');

    is( $dt->datetime, '1997-06-30T22:59:60', '-0100 time leap second T-0' );

    $dt->set_time_zone('UTC');

    is( $dt->datetime, '1997-06-30T23:59:60', 'UTC time leap second T-0' );
}

{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 23, minute => 0, second => 0,
                           time_zone => '-0100');

    is( $dt->datetime, '1997-06-30T23:00:00', '-0100 time leap second T+1' );

    $dt->set_time_zone('UTC');

    is( $dt->datetime, '1997-07-01T00:00:00', 'UTC time leap second T+1' );
}

{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 23, minute => 59, second => 59,
                           time_zone => 'UTC');

    is( $dt->datetime, '1997-06-30T23:59:59', 'UTC time leap second T-1' );

    $dt->set_time_zone('+0100');

    is( $dt->datetime, '1997-07-01T00:59:59', '+0100 time leap second T-1' );
}

{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 23, minute => 59, second => 60,
                           time_zone => 'UTC');

    is( $dt->datetime, '1997-06-30T23:59:60', 'UTC time leap second T-0' );

    $dt->set_time_zone('+0100');

    is( $dt->datetime, '1997-07-01T00:59:60', '+0100 time leap second T-0' );
}

{
    my $dt = DateTime->new(year => 1997, month => 7, day => 1,
                           hour => 0, minute => 0, second => 0,
                           time_zone => 'UTC');

    is( $dt->datetime, '1997-07-01T00:00:00', 'UTC time leap second T+1' );

    $dt->set_time_zone('+0100');

    is( $dt->datetime, '1997-07-01T01:00:00', '+0100 time leap second T+1' );
}

{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 23, minute => 59, second => 59,
                           time_zone => 'UTC');

    is( $dt->datetime, '1997-06-30T23:59:59', 'UTC time end of leap second day' );

    $dt->set_time_zone('+0100');

    is( $dt->datetime, '1997-07-01T00:59:59', '+0100 time end of leap second day' );
}


{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 23, minute => 59, second => 59,
                           time_zone => 'UTC');

    is( $dt->datetime, '1997-06-30T23:59:59', 'UTC time leap second T-1' );

    $dt->set_time_zone('-0100');

    is( $dt->datetime, '1997-06-30T22:59:59', '-0100 time leap second T-1' );
}

{
    my $dt = DateTime->new(year => 1997, month => 6, day => 30,
                           hour => 23, minute => 59, second => 60,
                           time_zone => 'UTC');

    is( $dt->datetime, '1997-06-30T23:59:60', 'UTC time leap second T-0' );

    $dt->set_time_zone('-0100');

    is( $dt->datetime, '1997-06-30T22:59:60', '-0100 time leap second T-0' );
}

{
    my $dt = DateTime->new(year => 1997, month => 7, day => 1,
                           hour => 0, minute => 0, second => 0,
                           time_zone => 'UTC');

    is( $dt->datetime, '1997-07-01T00:00:00', 'UTC time leap second T+1' );

    $dt->set_time_zone('-0100');

    is( $dt->datetime, '1997-06-30T23:00:00', '-0100 time leap second T+1' );
}

{
    my $dt = DateTime->new(year => 2005, month => 12, day => 31,
                           hour => 23, minute => 59, second => 60,
                           time_zone => 'UTC');

    is( $dt->second, 60, 'leap second at end of 2005 is allowed' );
}

{
    my $dt = DateTime->new( year => 2005, month => 12, day => 31,
                            hour => 23, minute => 59, second => 59,
                            time_zone => 'UTC',
                          );

    $dt->add( seconds => 1 );
    is( $dt->datetime, '2005-12-31T23:59:60', 'dt is 2005-12-31T23:59:60' );

    $dt->add( seconds => 1 );
    is( $dt->datetime, '2006-01-01T00:00:00', 'dt is 2006-01-01T00:00:00' );
}

# bug reported by Mike Schilli - addition got "stuck" at 60 seconds
# and never rolled over to the following day
{
    my $dt = DateTime->new( year => 2005, month => 12, day => 31,
                            hour => 23, minute => 59, second => 59,
                            time_zone => 'UTC',
                          );

    $dt->add( seconds => 1 );
    is( $dt->datetime, '2005-12-31T23:59:60', 'dt is 2005-12-31T23:59:60' );

    $dt->add( seconds => 1 );
    is( $dt->datetime, '2006-01-01T00:00:00', 'dt is 2006-01-01T00:00:00' );
}

# and this makes sure that fix for the above bug didn't break
# _non-leapsecond_ second addition
{
    my $dt = DateTime->new( year => 2005, month => 12, day => 30,
                            hour => 23, minute => 59, second => 58,
                            time_zone => 'UTC',
                          );

    $dt->add( seconds => 1 );
    is( $dt->datetime, '2005-12-30T23:59:59', 'dt is 2005-12-30T23:59:59' );

    $dt->add( seconds => 1 );
    is( $dt->datetime, '2005-12-31T00:00:00', 'dt is 2005-12-31T00:00:00' );
}

{
    for my $date ( [ 1972,  6, 30 ],
                   [ 1972, 12, 31 ],
                   [ 1973, 12, 31 ],
                   [ 1974, 12, 31 ],
                   [ 1975, 12, 31 ],
                   [ 1976, 12, 31 ],
                   [ 1977, 12, 31 ],
                   [ 1978, 12, 31 ],
                   [ 1979, 12, 31 ],
                   [ 1981,  6, 30 ],
                   [ 1982,  6, 30 ],
                   [ 1983,  6, 30 ],
                   [ 1985,  6, 30 ],
                   [ 1987, 12, 31 ],
                   [ 1989, 12, 31 ],
                   [ 1990, 12, 31 ],
                   [ 1992,  6, 30 ],
                   [ 1993,  6, 30 ],
                   [ 1994,  6, 30 ],
                   [ 1995, 12, 31 ],
                   [ 1997,  6, 30 ],
                   [ 1998, 12, 31 ],
                   [ 2005, 12, 31 ],
                   [ 2008, 12, 31 ],
                 )
    {
        my $dt = eval { DateTime->new( year   => $date->[0],
                                       month  => $date->[1],
                                       day    => $date->[2],
                                       hour   => 23,
                                       minute => 59,
                                       second => 60,
                                       time_zone => 'UTC',
                                     ) };

        my $formatted = join '-', @{ $date };
        ok( $dt, "We can make a DateTime object for the leap second on $formatted" );
    }
}
