#!/usr/bin/perl -w

use strict;

use Test::More tests => 57;

use DateTime;


{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 0, minute => 59, second => 58,
                           time_zone => '+0100',
                         );

    is( $t->second, 58, 'second value for leap second T-2, +0100' );

    is( $t->{utc_rd_days}, 720074,
        'UTC RD days for leap second T-2' );
    is( $t->{utc_rd_secs}, 86398,
        'UTC RD seconds for leap second T-2' );

    is( $t->{local_rd_days}, 720075,
        'local RD days for leap second T-2' );
    is( $t->{local_rd_secs}, 3598,
        'local RD seconds for leap second T-2' );
}

{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 0, minute => 59, second => 59,
                           time_zone => '+0100',
                         );

    is( $t->second, 59, 'second value for leap second T-1, +0100' );
    is( $t->{utc_rd_days}, 720074,
        'UTC RD days for leap second T-1' );
    is( $t->{utc_rd_secs}, 86399,
        'UTC RD seconds for leap second T-1' );

    is( $t->{local_rd_days}, 720075,
        'local RD days for leap second T-1' );
    is( $t->{local_rd_secs}, 3599,
        'local RD seconds for leap second T-1' );
}

{
    my $t = eval { DateTime->new( year => 1972, month => 7, day => 1,
                                  hour => 0, minute => 59, second => 60,
                                  time_zone => '+0100',
                                ) };

    ok( ! $@, 'constructor for second = 60' );

 SKIP:
    {
        skip 'constructor failed - no object to test', 5
            unless $t;

        is( $t->second, 60, 'second value for leap second T-0, +0100' );
        is( $t->{utc_rd_days}, 720074,
            'UTC RD days for leap second T-0' );
        is( $t->{utc_rd_secs}, 86400,
            'UTC RD seconds for leap second T-0' );

        is( $t->{local_rd_days}, 720075,
            'local RD days for leap second T-0' );
        is( $t->{local_rd_secs}, 3600,
            'local RD seconds for leap second T-0' );
    }
}

{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 1, minute => 0, second => 0,
                           time_zone => '+0100',
                         );

    is( $t->second, 0, 'second value for leap second T+1, +0100' );
    is( $t->{utc_rd_days}, 720075,
        'UTC RD days for leap second T+1' );
    is( $t->{utc_rd_secs}, 0,
        'UTC RD seconds for leap second T+1' );

    is( $t->{local_rd_days}, 720075,
        'local RD days for leap second T+1' );
    is( $t->{local_rd_secs}, 3601,
        'local RD seconds for leap second T+1' );
}


{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 1, minute => 0, second => 1,
                           time_zone => '+0100',
                         );

    is( $t->second, 1, 'second value for leap second T+2, +0100' );
    is( $t->{utc_rd_days}, 720075,
        'UTC RD days for leap second T+2' );
    is( $t->{utc_rd_secs}, 1,
        'UTC RD seconds for leap second T+2' );

    is( $t->{local_rd_days}, 720075,
        'local RD days for leap second T+2' );
    is( $t->{local_rd_secs}, 3602,
        'local RD seconds for leap second T+2' );
}

{
    my $t = DateTime->new( year => 1972, month => 7, day => 1,
                           hour => 23, minute => 59, second => 59,
                           time_zone => '+0100',
                         );

    is( $t->second, 59, 'second value for end of leap second day, +0100' );
    is( $t->{utc_rd_days}, 720075,
        'UTC RD days for end of leap second day' );
    is( $t->{utc_rd_secs}, 82799,
        'UTC RD seconds for end of leap second day' );

    is( $t->{local_rd_days}, 720075,
        'local RD days for leap second day' );
    is( $t->{local_rd_secs}, 86400,
        'local RD seconds for end of leap second day' );
}

{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 22, minute => 59, second => 58,
                           time_zone => '-0100',
                         );

    is( $t->second, 58, 'second value for leap second T-2, -0100' );

    is( $t->{utc_rd_days}, 720074,
        'UTC RD days for leap second T-2' );
    is( $t->{utc_rd_secs}, 86398,
        'UTC RD seconds for leap second T-2' );

    is( $t->{local_rd_days}, 720074,
        'local RD days for leap second T-2' );
    is( $t->{local_rd_secs}, 82798,
        'local RD seconds for leap second T-2' );
}

{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 22, minute => 59, second => 59,
                           time_zone => '-0100',
                         );

    is( $t->second, 59, 'second value for leap second T-1, -0100' );

    is( $t->{utc_rd_days}, 720074,
        'UTC RD days for leap second T-1' );
    is( $t->{utc_rd_secs}, 86399,
        'UTC RD seconds for leap second T-1' );

    is( $t->{local_rd_days}, 720074,
        'local RD days for leap second T-1' );
    is( $t->{local_rd_secs}, 82799,
        'local RD seconds for leap second T-1' );
}

{
    my $t = eval { DateTime->new( year => 1972, month => 6, day => 30,
                                  hour => 22, minute => 59, second => 60,
                                  time_zone => '-0100',
                                ) };

    ok( ! $@, 'constructor for second = 60' );

 SKIP:
    {
        skip 'constructor failed - no object to test', 5
            unless $t;

        is( $t->second, 60, 'second value for leap second T-0, -0100' );

        is( $t->{utc_rd_days}, 720074,
            'UTC RD days for leap second T-0' );
        is( $t->{utc_rd_secs}, 86400,
            'UTC RD seconds for leap second T-0' );

        is( $t->{local_rd_days}, 720074,
            'local RD days for leap second T-0' );
        is( $t->{local_rd_secs}, 82800,
        'local RD seconds for leap second T-0' );
    }
}

{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 23, minute => 0, second => 0,
                           time_zone => '-0100',
                         );

    is( $t->second, 0, 'second value for leap second T+1, -0100' );

    is( $t->{utc_rd_days}, 720075,
        'UTC RD days for leap second T+1' );
    is( $t->{utc_rd_secs}, 0,
        'UTC RD seconds for leap second T+1' );

    is( $t->{local_rd_days}, 720074,
        'local RD days for leap second T+1' );
    is( $t->{local_rd_secs}, 82801,
        'local RD seconds for leap second T+1' );
}


{
    my $t = DateTime->new( year => 1972, month => 6, day => 30,
                           hour => 23, minute => 0, second => 1,
                           time_zone => '-0100',
                         );

    is( $t->second, 1, 'second value for leap second T+2, -0100' );

    is( $t->{utc_rd_days}, 720075,
        'UTC RD days for leap second T+2' );
    is( $t->{utc_rd_secs}, 1,
        'UTC RD seconds for leap second T+2' );

    is( $t->{local_rd_days}, 720074,
        'local RD days for leap second T+2' );
    is( $t->{local_rd_secs}, 82802,
        'local RD seconds for leap second T+2' );
}
