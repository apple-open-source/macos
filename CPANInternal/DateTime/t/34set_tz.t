#!/usr/bin/perl -w

use strict;

use Test::More tests => 8;

use DateTime;

# These tests are for a bug related to a bad interaction between the
# horrid ->_handle_offset_modifier method and calling ->set_time_zone
# on a real Olson time zone.  When _handle_offset_modifier was called
# from set_time_zone, it tried calling ->_offset_for_local_datetime,
# which was bogus, because at that point it doesn't know the local
# date time any more, only UTC.
#
# The fix is to have ->_handle_offset_modifier call ->offset when it
# knows that UTC is valid, which is determined by an arg to
# ->_handle_offset_modifier

# These tests come from one of the zdump-generated test files in
# DT::TZ
{
    my $dt = DateTime->new( year => 1934, month => 2, day => 26,
                            hour => 0, minute => 59, second => 59,
                            time_zone => 'UTC',
                           );
    $dt->set_time_zone( 'Africa/Niamey' );

    is( $dt->year, 1934, 'local year should be 1934 (1934-02-25 23:59:59)' );
    is( $dt->month, 2, 'local month should be 2 (1934-02-25 23:59:59)' );
    is( $dt->day, 25, 'local day should be 25 (1934-02-25 23:59:59)' );
    is( $dt->hour, 23, 'local hour should be 23 (1934-02-25 23:59:59)' );
    is( $dt->minute, 59, 'local minute should be 59 (1934-02-25 23:59:59)' );
    is( $dt->second, 59, 'local second should be 59 (1934-02-25 23:59:59)' );

    is( $dt->is_dst, 0, 'is_dst should be 0 (1934-02-25 23:59:59)' );
    is( $dt->offset, -3600, 'offset should be -3600 (1934-02-25 23:59:59)' );
}

