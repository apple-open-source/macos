#!/usr/bin/perl -w

use strict;

use Test::More tests => 12;

use DateTime;

{
    my $dt = DateTime->new( year => 2000,
                            hour => 1,
                            nanosecond => 500,
                            time_zone => 'UTC',
                          );

    my ( $utc_rd_days, $utc_rd_secs, $utc_nanosecs ) = $dt->utc_rd_values;

    is( $utc_rd_days, 730120, 'utc rd days is 730120' );
    is( $utc_rd_secs, 3600, 'utc rd seconds is 3600' );
    is( $utc_nanosecs, 500, 'nanoseconds is 500' );

    my ( $local_rd_days, $local_rd_secs, $local_nanosecs ) = $dt->local_rd_values;

    is( $local_rd_days, $utc_rd_days, 'local & utc rd days are equal' );
    is( $local_rd_secs, $utc_rd_secs, 'local & utc rd seconds are equal' );
    is( $local_nanosecs, $utc_nanosecs, 'local & UTC nanoseconds are equal' );
}

{
    my $dt = DateTime->new( year => 2000,
                            hour => 1,
                            nanosecond => 500,
                            time_zone => '+02:00',
                          );

    my ( $utc_rd_days, $utc_rd_secs, $utc_nanosecs ) = $dt->utc_rd_values;

    is( $utc_rd_days, 730119, 'utc rd days is 730119' );
    is( $utc_rd_secs, 82800, 'utc rd seconds is 82800' );
    is( $utc_nanosecs, 500, 'nanoseconds is 500' );

    my ( $local_rd_days, $local_rd_secs, $local_nanosecs ) = $dt->local_rd_values;

    is( $local_rd_days, 730120, 'local rd days is 730120' );
    is( $local_rd_secs, 3600, 'local rd seconds is 3600' );
    is( $local_nanosecs, 500, 'local nanoseconds is 500' );
}
