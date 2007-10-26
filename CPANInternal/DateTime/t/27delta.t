#!/usr/bin/perl -w

use strict;

use Test::More tests => 38;

use DateTime;

{
    my $date1 = DateTime->new( year => 2001, month => 5, day => 10,
			       hour => 4, minute => 3, second => 2,
			       nanosecond => 12,
			       time_zone => 'UTC' );

    my $date2 = DateTime->new( year => 2001, month => 6, day => 12,
			       hour => 5, minute => 7, second => 23,
			       nanosecond => 7,
			       time_zone => 'UTC' );

    {
        my $dur_md = $date2->delta_md($date1);

        is( $dur_md->delta_months,  1, 'delta_md months is 1' );
        is( $dur_md->delta_days,    2, 'delta_md days is 2' );
        is( $dur_md->delta_minutes, 0, 'delta_md minutes is 0' );
        is( $dur_md->delta_seconds, 0, 'delta_md seconds is 0' );
        is( $dur_md->delta_nanoseconds, 0, 'delta_md nanoseconds is 0' );

        my $dur_d = $date2->delta_days($date1);

        is( $dur_d->delta_months,  0, 'delta_d months is 0' );
        is( $dur_d->delta_days,   33, 'delta_d days is 33' );
        is( $dur_d->delta_minutes, 0, 'delta_d minutes is 0' );
        is( $dur_d->delta_seconds, 0, 'delta_d seconds is 0' );
        is( $dur_d->delta_nanoseconds, 0, 'delta_d nanoseconds is 0' );

        my $dur_ms = $date2->delta_ms($date1);

        is( $dur_ms->delta_months,       0, 'delta_ms months is 0' );
        is( $dur_ms->delta_days,         0, 'delta_ms days is 0' );
        is( $dur_ms->delta_minutes,  47584, 'delta_ms minutes is 47584' );
        is( $dur_ms->delta_seconds,     20, 'delta_ms seconds is 20' );
        is( $dur_ms->delta_nanoseconds,  0, 'delta_ms nanoseconds is 0' );

        is( $dur_ms->hours, 793, 'hours is 793' );
    }

    {
        my $dur_md = $date1->delta_md($date2);

        is( $dur_md->delta_months,   1, 'delta_md months is 1' );
        is( $dur_md->delta_days,     2, 'delta_md days is 2' );
        is( $dur_md->delta_minutes,  0, 'delta_md minutes is 0' );
        is( $dur_md->delta_seconds,  0, 'delta_md seconds is 0' );
        is( $dur_md->delta_nanoseconds, 0, 'delta_md nanoseconds is 0' );

        my $dur_d = $date1->delta_days($date2);

        is( $dur_d->delta_months,   0, 'delta_d months is 0' );
        is( $dur_d->delta_days,    33, 'delta_d days is 33' );
        is( $dur_d->delta_minutes,  0, 'delta_d minutes is 0' );
        is( $dur_d->delta_seconds,  0, 'delta_d seconds is 0' );
        is( $dur_d->delta_nanoseconds, 0, 'delta_d nanoseconds is 0' );

        my $dur_ms = $date1->delta_ms($date2);

        is( $dur_ms->delta_months,       0, 'delta_ms months is 0' );
        is( $dur_ms->delta_days,         0, 'delta_ms days is 0' );
        is( $dur_ms->delta_minutes,  47584, 'delta_ms minutes is 47584' );
        is( $dur_ms->delta_seconds,     20, 'delta_ms seconds is 20' );
        is( $dur_ms->delta_nanoseconds,  0, 'delta_ms nanoseconds is 0' );

        is( $dur_ms->hours, 793, 'hours is 793' );
    }
}

{
    my $date1 = DateTime->new( year => 2001, month => 5, day => 10,
			       hour => 15, minute => 0, second => 0,
			       time_zone => 'UTC' );

    my $date2 = DateTime->new( year => 2001, month => 5, day => 11,
			       hour => 12, minute => 30, second => 10,
			       time_zone => 'UTC' );

    my $dur_ms = $date1->delta_ms($date2);

    is( $dur_ms->delta_months,       0, 'delta_ms months is 0' );
    is( $dur_ms->delta_days,         0, 'delta_ms days is 0' );
    is( $dur_ms->delta_minutes,   1290, 'delta_ms minutes is 1290' );
    is( $dur_ms->delta_seconds,     10, 'delta_ms seconds is 30' );
    is( $dur_ms->delta_nanoseconds,  0, 'delta_ms nanoseconds is 0' );

    is( $dur_ms->hours, 21, 'hours is 21' );
}
