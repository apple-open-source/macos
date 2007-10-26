#!/usr/bin/perl -w

use strict;

use Test::More tests => 4;

use DateTime;


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

    my $dt = DateTime->new( year => 2003, month => 4, day => 5,
                            hour => 2,
                            time_zone => 'America/Chicago',
                          );

    eval { $dt->add( days => 1 ) };
    like( $@, qr/Invalid local time .+/, 'exception for invalid time produced via add' );
}
