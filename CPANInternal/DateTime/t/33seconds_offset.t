#!/usr/bin/perl -w

use strict;

use Test::More tests => 6;

use DateTime;

{
    my $dt = DateTime->new( year => 1997, month => 6, day => 30,
                            hour => 23, minute => 58, second => 59,
                            time_zone => 'UTC' );

    $dt->set_time_zone('+00:00:30');

    is( $dt->datetime, '1997-06-30T23:59:29', '+00:00:30 leap second T-61' );
}

{
    my $dt = DateTime->new( year => 1997, month => 6, day => 30,
                            hour => 23, minute => 59, second => 29,
                            time_zone => 'UTC' );

    $dt->set_time_zone('+00:00:30');

    is( $dt->datetime, '1997-06-30T23:59:59', '+00:00:30 leap second T-31' );
}

{
    local $TODO = 'offsets with seconds are broken near leap seconds';

    my $dt = DateTime->new( year => 1997, month => 6, day => 30,
                            hour => 23, minute => 59, second => 30,
                            time_zone => 'UTC' );

    $dt->set_time_zone('+00:00:30');

    is( $dt->datetime, '1997-06-30T23:59:60', '+00:00:30 leap second T-30' );
}

{
    local $TODO = 'offsets with seconds are broken near leap seconds';

    my $dt = DateTime->new( year => 1997, month => 6, day => 30,
                            hour => 23, minute => 59, second => 31,
                            time_zone => 'UTC' );

    $dt->set_time_zone('+00:00:30');

    is( $dt->datetime, '1997-07-01T00:00:00', '+00:00:30 leap second T-29' );
}

{
    local $TODO = 'offsets with seconds are broken near leap seconds';

    my $dt = DateTime->new( year => 1997, month => 6, day => 30,
                            hour => 23, minute => 59, second => 60,
                            time_zone => 'UTC' );

    $dt->set_time_zone('+00:00:30');

    is( $dt->datetime, '1997-07-01T00:00:30', '+00:00:30 leap second T-0' );
}

{
    my $dt = DateTime->new( year => 1997, month => 7, day => 1,
                            hour => 0, minute => 0, second => 0,
                            time_zone => 'UTC' );

    $dt->set_time_zone('+00:00:30');

    is( $dt->datetime, '1997-07-01T00:00:30', '+00:00:30 leap second T+1' );
}
