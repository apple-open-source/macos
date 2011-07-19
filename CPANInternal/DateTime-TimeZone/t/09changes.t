use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 101;

# The point of this group of tests is to try to check that DST changes
# are occuring at exactly the right time in various time zones.  It's
# important to check both pre-generated spans, as well as spans that
# have to be generated on the fly.

# Rule	AN	1996	max	-	Mar	lastSun	2:00s	0	-
# Rule	AN	2000	only	-	Aug	lastSun	2:00s	1:00	-
# Rule	AN	2001	max	-	Oct	lastSun	2:00s	1:00	-
# Zone	NAME		GMTOFF	RULES	FORMAT	[UNTIL]
# Zone Australia/Sydney	10:04:52 -	LMT	1895 Feb
# 			10:00	Aus	EST	1971
# 			10:00	AN	EST

{
    # one minute before change to standard time
    my $dt = DateTime->new( year => 1997, month => 3, day => 29,
                            hour => 15, minute => 59,
                            time_zone => 'UTC' );

    $dt->set_time_zone('Australia/Sydney');

    is( $dt->hour, 2, 'A/S 1997: hour should be 2' );

    $dt->set_time_zone('UTC')->add( minutes => 1 )->set_time_zone('Australia/Sydney');

    is( $dt->hour, 2, 'A/S 1997: hour should still be 2' );
}

# same tests without using UTC as intermediate
{
    # Can't start at 1:59 or we get the _2nd_ 1:59 of that day (post-DST change)
    my $dt = DateTime->new( year => 1997, month => 3, day => 30,
                            hour => 1, minute => 59,
                            time_zone => 'Australia/Sydney' );

    $dt->add( hours => 1 );

    is( $dt->hour, 2, 'A/S 1997: hour should be 2' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 2, 'A/S 1997: hour should still be 2' );
}

{
    # one minute before change to standard time
    my $dt = DateTime->new( year => 2002, month => 10, day => 26,
                            hour => 15, minute => 59,
                            time_zone => 'UTC' );

    $dt->set_time_zone('Australia/Sydney');

    is( $dt->hour, 1, 'A/S 2002: hour should be 1' );

    $dt->set_time_zone('UTC')->add( minutes => 1 )->set_time_zone('Australia/Sydney');

    is( $dt->hour, 3, 'A/S 2002: hour should be 3' );

}

# same tests without using UTC as intermediate
{
    my $dt = DateTime->new( year => 2002, month => 10, day => 27,
                            hour => 1, minute => 59,
                            time_zone => 'Australia/Sydney' );

    is( $dt->hour, 1, 'A/S 2002: hour should be 1' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 3, 'A/S 2002: hour should be 3' );
}

# do same tests with future dates so more data is generated
{
    # Can't start at 1:59 or we get the _2nd_ 1:59 of that day (post-DST change)
    my $dt = DateTime->new( year => 2040, month => 4, day => 1,
                            hour => 1, minute => 59,
                            time_zone => 'Australia/Sydney' );

    $dt->add( hours => 1 );

    is( $dt->hour, 2, 'A/S 2040: hour should be 2' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 2, 'A/S 2040: hour should still be 2' );
}

{
    my $dt = DateTime->new( year => 2040, month => 10, day => 7,
                            hour => 1, minute => 59,
                            time_zone => 'Australia/Sydney' );

    is( $dt->hour, 1, 'A/S 2040: hour should be 1' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 3, 'A/S 2040: hour should be 3' );
}

# Rule	EU	1981	max	-	Mar	lastSun	 1:00u	1:00	S
# Rule	EU	1996	max	-	Oct	lastSun	 1:00u	0	-
{
    # one minute before change to standard time
    my $dt = DateTime->new( year => 1982, month => 3, day => 28,
                            hour => 0, minute => 59,
                            time_zone => 'UTC' );

    $dt->set_time_zone('Europe/Vienna');

    is( $dt->hour, 1, 'E/V 1982: hour should be 1' );

    $dt->set_time_zone('UTC')->add( minutes => 1 )->set_time_zone('Europe/Vienna');

    is( $dt->hour, 3, 'E/V 1982: hour should be 3' );
}

# same tests without using UTC as intermediate
{
    # wrapped in eval because if change data is buggy it can throw exception
    my $dt = DateTime->new( year => 1982, month => 3, day => 28,
                            hour => 1, minute => 59,
                            time_zone => 'Europe/Vienna' );

    is( $dt->hour, 1, 'E/V 1982: hour should be 1' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 3, 'E/V 1982: hour should be 3' );
}

{
    # one minute before change to standard time
    my $dt = DateTime->new( year => 1997, month => 10, day => 26,
                            hour => 0, minute => 59,
                            time_zone => 'UTC' );

    $dt->set_time_zone('Europe/Vienna');

    is( $dt->hour, 2, 'E/V 1997: hour should be 2' );

    $dt->set_time_zone('UTC')->add( minutes => 1 )->set_time_zone('Europe/Vienna');

    is( $dt->hour, 2, 'E/V 1997: hour should still be 2' );

}

# same tests without using UTC as intermediate
{
    # can't be created directly because of overlap between changes
    my $dt = DateTime->new( year => 1997, month => 10, day => 26,
                            hour => 1, minute => 59,
                            time_zone => 'Europe/Vienna' );

    $dt->add( hours => 1 );

    is( $dt->hour, 2, 'E/V 1997: hour should be 2' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 2, 'E/V 1997: hour should still be 2' );
}

# future
{
    my $dt = DateTime->new( year => 2040, month => 3, day => 25,
                            hour => 1, minute => 59,
                            time_zone => 'Europe/Vienna' );

    is( $dt->hour, 1, 'E/V 2040: hour should be 1' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 3, 'E/V 2040: hour should be 3' );
}

{
    my $dt = DateTime->new( year => 2040, month => 10, day => 28,
                            hour => 1, minute => 59,
                            time_zone => 'Europe/Vienna' );

    $dt->add( hours => 1 );

    is( $dt->hour, 2, 'E/V 2040: hour should be 2' );

    $dt->add( minutes => 1 );

    is( $dt->hour, 2, 'E/V 2040: hour should still be 2' );
}

# Africa/Algiers has an observance that ends at 1977-10-21T00:00:00
# local time and a rule that starts at exactly the same time

# Rule	Algeria	1977	only	-	May	 6	 0:00	1:00	S
# Rule	Algeria	1977	only	-	Oct	21	 0:00	0	-
#
# 			0:00	Algeria	WE%sT	1977 Oct 21
# 			1:00	Algeria	CE%sT	1979 Oct 26
{
    my $dt = DateTime->new( year => 1977, month => 10, day => 20,
                            hour => 23, minute => 59,
                            time_zone => 'Africa/Algiers'
                          );

    is( $dt->time_zone_short_name, 'WEST', 'short name is WEST' );
    is( $dt->is_dst, 1, 'is dst' );

    # observance ends, new rule starts, net effect is same offset,
    # different short name, no longer is DST
    $dt->add( minutes => 1 );

    is( $dt->time_zone_short_name, 'CET', 'short name is CET' );
    is( $dt->is_dst, 0, 'is not dst' );
}

{
    my $dt = DateTime->new( year => 2000, month => 10, day => 5,
                            hour => 15, time_zone => 'America/Chicago',
                          );
    is( $dt->hour, 15, 'hour is 15' );
    is( $dt->offset, -18000, 'offset is -18000' );
    is( $dt->is_dst, 1, 'is dst' );

    $dt->set_time_zone( 'America/New_York' );
    is( $dt->offset, -14400, 'offset is -14400' );
    is( $dt->is_dst, 1, 'is dst' );
    is( $dt->hour, 16,
        'America/New_York is exactly one hour later than America/Chicago - hour' );
    is( $dt->minute, 0,
        'America/New_York is exactly one hour later than America/Chicago - minute' );
    is( $dt->second, 0,
        'America/New_York is exactly one hour later than America/Chicago - second' );
}

{
    # this is the second of the two 01:59:59 times for that date
    my $dt = DateTime->new( year => 2003, month => 10, day => 26,
                            hour => 1, minute => 59, second => 59,
                            time_zone => 'America/Chicago',
                          );

    is( $dt->offset, -21600, 'offset should be -21600' );
    is( $dt->is_dst, 0, 'is not dst' );

    $dt->subtract( hours => 1 );

    is( $dt->offset, -18000, 'offset should be -18000' );
    is( $dt->is_dst, 1, 'is not dst' );
    is( $dt->hour, 1, "crossing DST bounday does not change local hour" );
}

{
    my $dt = DateTime->new( year => 2003, month => 10, day => 26,
                            hour => 2, time_zone => 'America/Chicago',
                          );

    is( $dt->offset, -21600, 'offset should be -21600' );
}

{
    my $dt = DateTime->new( year => 2003, month => 10, day => 26,
                            hour => 3, time_zone => 'America/Chicago',
                          );

    is( $dt->offset, -21600, 'offset should be -21600' );
}

{
    eval
    {
        DateTime->new( year => 2003, month => 4, day => 6,
                       hour => 2, time_zone => 'America/Chicago',
                     )
    };

    like( $@, qr/Invalid local time .+/, 'exception for invalid time' );

    eval
    {
        DateTime->new( year => 2003, month => 4, day => 6,
                       hour => 2, minute => 59, second => 59,
                       time_zone => 'America/Chicago',
                     );
    };
    like( $@, qr/Invalid local time .+/, 'exception for invalid time' );
}

{
    eval
    {
        DateTime->new( year => 2003, month => 4, day => 6,
                       hour => 1, minute => 59, second => 59,
                       time_zone => 'America/Chicago',
                     );
    };
    ok( ! $@, 'no exception for valid time' );

 SKIP:
    {
        skip "DateTime 0.29 has a date math bug that causes this test to fail", 1
            if ( DateTime->VERSION >= 0.29 && DateTime->VERSION < 0.30 );

        my $dt = DateTime->new( year => 2003, month => 4, day => 5,
                                hour => 2,
                                time_zone => 'America/Chicago',
                              );

        eval { $dt->add( days => 1 ) };
        like( $@, qr/Invalid local time .+/, 'exception for invalid time produced via add' );
    }
}

{
    my $dt = DateTime->new( year => 2003, month => 4, day => 5,
                            hour => 2,
                            time_zone => 'America/Chicago',
                          );
    eval { $dt->add( hours => 24 ) };
    ok( ! $@, 'add 24 hours should work even if add 1 day does not' );

    is( $dt->hour, 3, "hour should no be 3" );
}

{
    my $dt = DateTime->new( year => 2003, month => 4, day => 6,
                            hour => 3, time_zone => 'America/Chicago',
                          );

    is( $dt->hour, 3, 'hour should be 3' );
    is( $dt->offset, -18000, 'offset should be -18000' );

    $dt->subtract( seconds => 1 );

    is( $dt->hour, 1, 'hour should be 1' );
    is( $dt->offset, -21600, 'offset should be -21600' );
}

{
    my $dt = DateTime->new( year => 2003, month => 4, day => 6,
                            hour => 3, time_zone => 'floating',
                          );
    $dt->set_time_zone( 'America/Chicago' );

    is( $dt->hour, 3, 'hour should be 3 after switching from floating TZ' );
    is( $dt->offset, -18000,
        'tz offset should be -18000' );
}

{
    my $dt = DateTime->new( year => 2003, month => 4, day => 6,
                            hour => 3, time_zone => 'America/Chicago',
                          );
    $dt->set_time_zone( 'floating' );

    is( $dt->hour, 3, 'hour should be 3 after switching to floating TZ' );
    is( $dt->local_rd_as_seconds - $dt->utc_rd_as_seconds, 0,
        'tz offset should be 0' );
}

{
    eval
    {
        DateTime->new( year => 2040, month => 3, day => 11,
                       hour => 2, minute => 59, second => 59,
                       time_zone => 'America/Chicago',
                     );
    };
    like( $@, qr/Invalid local time .+/, 'exception for invalid time' );
}

{
    my $dt =
        DateTime->new( year => 2001, month => 10, day => 28,
                       hour => 0, minute => 59,
                       time_zone => 'UTC' );

    $dt->set_time_zone('Europe/Vienna');

    is( $dt->hour, 2, 'hour should be 2 in vienna at 00:59:00 UTC' );

    $dt->set_time_zone('UTC')->add( minutes => 1 )->set_time_zone('Europe/Vienna');

    is( $dt->hour, 2, 'hour should be 2 in vienna at 01:00:00 UTC' );
}

{
    # Doing this triggered a recursion bug in earlier versions of
    # DateTime::TimeZone.
    local $ENV{TZ} = 'America/Chicago';

    my $local_tz = DateTime::TimeZone->new( name => 'America/Chicago' );
    my $utc_tz   = DateTime::TimeZone->new( name => 'UTC' );

    my $dt = DateTime->new( year => 2050, time_zone => $local_tz );

    my $sixm = DateTime::Duration->new( months => 6 );
    foreach ( [ 2050, 7, 1, 1, 'CDT' ],
              [ 2051, 1, 1, 0, 'CST' ],
              [ 2051, 7, 1, 1, 'CDT' ],
              [ 2052, 1, 1, 0, 'CST' ],
              [ 2052, 7, 1, 1, 'CDT' ],
              [ 2053, 1, 1, 0, 'CST' ],
              [ 2053, 7, 1, 1, 'CDT' ],
              [ 2054, 1, 1, 0, 'CST' ],
              [ 2054, 7, 1, 1, 'CDT' ],
              [ 2055, 1, 1, 0, 'CST' ],
              [ 2055, 7, 1, 1, 'CDT' ],
              [ 2056, 1, 1, 0, 'CST' ],
              [ 2056, 7, 1, 1, 'CDT' ],
              [ 2057, 1, 1, 0, 'CST' ],
              [ 2057, 7, 1, 1, 'CDT' ],
              [ 2058, 1, 1, 0, 'CST' ],
              [ 2058, 7, 1, 1, 'CDT' ],
              [ 2059, 1, 1, 0, 'CST' ],
              [ 2059, 7, 1, 1, 'CDT' ],
              [ 2060, 1, 1, 0, 'CST' ],
              [ 2060, 7, 1, 1, 'CDT' ],
            )
    {
        $dt->set_time_zone($utc_tz);

        $dt->add_duration($sixm);

        $dt->set_time_zone($local_tz);

        $_->[1] = sprintf( '%02d', $_->[1] );

        my $expect = join ' ', @$_;

        is( $dt->strftime( '%Y %m%e%k %Z' ), $expect,
            "datetime is $expect" );
    }
}

{
    my $local_tz = DateTime::TimeZone->new( name => 'America/New_York' );
    my $utc_tz   = DateTime::TimeZone->new( name => 'UTC' );

    my $dt = DateTime->new( year => 2060, time_zone => $local_tz );

    my $neg_sixm = DateTime::Duration->new( months => -6 );
    foreach ( [ 2059, 7, 1, 1, 'EDT' ],
              [ 2059, 1, 1, 0, 'EST' ],
              [ 2058, 7, 1, 1, 'EDT' ],
              [ 2058, 1, 1, 0, 'EST' ],
              [ 2057, 7, 1, 1, 'EDT' ],
              [ 2057, 1, 1, 0, 'EST' ],
              [ 2056, 7, 1, 1, 'EDT' ],
              [ 2056, 1, 1, 0, 'EST' ],
              [ 2055, 7, 1, 1, 'EDT' ],
              [ 2055, 1, 1, 0, 'EST' ],
              [ 2054, 7, 1, 1, 'EDT' ],
              [ 2054, 1, 1, 0, 'EST' ],
              [ 2053, 7, 1, 1, 'EDT' ],
              [ 2053, 1, 1, 0, 'EST' ],
              [ 2052, 7, 1, 1, 'EDT' ],
              [ 2052, 1, 1, 0, 'EST' ],
              [ 2051, 7, 1, 1, 'EDT' ],
              [ 2051, 1, 1, 0, 'EST' ],
              [ 2050, 7, 1, 1, 'EDT' ],
              [ 2050, 1, 1, 0, 'EST' ],
            )
    {
        $dt->set_time_zone($utc_tz);

        $dt->add_duration($neg_sixm);

        $dt->set_time_zone($local_tz);

        $_->[1] = sprintf( '%02d', $_->[1] );

        my $expect = join ' ', @$_;

        is( $dt->strftime( '%Y %m%e%k %Z' ), $expect,
            "datetime is $expect" );
    }
}
