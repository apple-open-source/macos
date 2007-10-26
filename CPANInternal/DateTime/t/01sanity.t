#!/usr/bin/perl -w

use strict;

use Test::More tests => 16;

use DateTime;

{
    my $dt = new DateTime( year => 1870, month => 10, day => 21,
                           hour => 12, minute => 10, second => 45,
                           nanosecond => 123456,
                           time_zone => 'UTC' );

    is( $dt->year, '1870', "Year accessor, outside of the epoch" );
    is( $dt->month, '10',  "Month accessor, outside the epoch" );
    is( $dt->day, '21',    "Day accessor, outside the epoch" );
    is( $dt->hour, '12',   "Hour accessor, outside the epoch" );
    is( $dt->minute, '10', "Minute accessor, outside the epoch" );
    is( $dt->second, '45', "Second accessor, outside the epoch" );
    is( $dt->nanosecond, '123456', "nanosecond accessor, outside the epoch" );

    $dt = DateTime->from_object( object => $dt );
    is( $dt->year, '1870', "Year should be identical" );
    is( $dt->month, '10',  "Month should be identical" );
    is( $dt->day, '21',    "Day should be identical" );
    is( $dt->hour, '12',   "Hour should be identical" );
    is( $dt->minute, '10', "Minute should be identical" );
    is( $dt->second, '45', "Second should be identical" );
    is( $dt->nanosecond, '123456', "nanosecond should be identical" );
}

{
    my $dt = DateTime->new( year => 1870, month => 10, day => 21,
                            hour => 12, minute => 10, second => 45,
                            time_zone => 'UTC' );
    is( $dt->minute, '10', "Minute accessor, outside the epoch" );
    is( $dt->second, '45', "Second accessor, outside the epoch" );
}
