#!/usr/bin/perl -w

use strict;

use Test::More tests => 127;

use DateTime;

# These tests should be the final word on dt subtraction involving a
# DST-changing time zone

{
    my $dt1 = DateTime->new( year => 2003, month => 5, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 11, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dur1 = $dt2->subtract_datetime($dt1);
    my %deltas1 = $dur1->deltas;
    is( $deltas1{months}, 6, 'delta_months is 6' );
    is( $deltas1{days}, 0, 'delta_days is 0' );
    is( $deltas1{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas1{seconds}, 0, 'delta_seconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur1), $dt2 ), 0,
        'subtract_datetime is reversible from start point' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur1), $dt1 ), 0,
        'subtract_datetime is reversible from end point' );
    is( $deltas1{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    my $dur2 = $dt1->subtract_datetime($dt2);
    my %deltas2 = $dur2->deltas;
    is( $deltas2{months}, -6, 'delta_months is -6' );
    is( $deltas2{days}, 0, 'delta_days is 0' );
    is( $deltas2{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas2{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas2{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    my $dur3 = $dt2->delta_md($dt1);
    my %deltas3 = $dur3->deltas;
    is( $deltas3{months}, 6, 'delta_months is 6' );
    is( $deltas3{days}, 0, 'delta_days is 0' );
    is( $deltas3{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas3{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas3{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur3), $dt2 ), 0,
        'delta_md is reversible from start point' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur3), $dt1 ), 0,
        'delta_md is reversible from end point' );

    my $dur4 = $dt2->delta_days($dt1);
    my %deltas4 = $dur4->deltas;
    is( $deltas4{months}, 0, 'delta_months is 0' );
    is( $deltas4{days}, 184, 'delta_days is 184' );
    is( $deltas4{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas4{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas4{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur3), $dt2 ), 0,
        'delta_days is reversible from start point' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur4), $dt1 ), 0,
        'delta_days is reversible from end point' );
}

# same as above, but now the UTC hour of the earlier datetime is
# _greater_ than that of the later one.  this checks that overflows
# are handled correctly.
{
    my $dt1 = DateTime->new( year => 2003, month => 5, day => 6, hour => 18,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 11, day => 6, hour => 18,
                             time_zone => 'America/Chicago',
                           );

    my $dur1 = $dt2->subtract_datetime($dt1);
    my %deltas1 = $dur1->deltas;
    is( $deltas1{months}, 6, 'delta_months is 6' );
    is( $deltas1{days}, 0, 'delta_days is 0' );
    is( $deltas1{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas1{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas1{nanoseconds}, 0, 'delta_nanoseconds is 0' );
}

# make sure delta_md and delta_days work in the face of DST change
# where we lose an hour
{
    my $dt1 = DateTime->new( year => 2003, month => 11, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2004, month => 5, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dur1 = $dt2->delta_md($dt1);
    my %deltas1 = $dur1->deltas;
    is( $deltas1{months}, 6, 'delta_months is 6' );
    is( $deltas1{days}, 0, 'delta_days is 0' );
    is( $deltas1{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas1{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas1{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    my $dur2 = $dt2->delta_days($dt1);
    my %deltas2 = $dur2->deltas;
    is( $deltas2{months}, 0, 'delta_months is 0' );
    is( $deltas2{days}, 182, 'delta_days is 182' );
    is( $deltas2{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas2{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas2{nanoseconds}, 0, 'delta_nanoseconds is 0' );

}

# the docs say use UTC to guarantee reversibility
{
    my $dt1 = DateTime->new( year => 2003, month => 5, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 11, day => 6,
                             time_zone => 'America/Chicago',
                           );

    $dt1->set_time_zone('UTC');
    $dt2->set_time_zone('UTC');

    my $dur = $dt2->subtract_datetime($dt1);

    is( DateTime->compare( $dt1->add_duration($dur), $dt2 ), 0,
        'subtraction is reversible from start point with UTC' );
    is( DateTime->compare( $dt2->subtract_duration($dur), $dt2 ), 0,
        'subtraction is reversible from start point with UTC' );
}

# The important thing here is that after a subtraction, we can use the
# duration to get from one date to the other, regardless of the type
# of subtraction done.
{
    my $dt1 = DateTime->new( year => 2003, month => 5, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 11, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dur1 = $dt2->subtract_datetime_absolute($dt1);

    my %deltas1 = $dur1->deltas;
    is( $deltas1{months}, 0, 'delta_months is 0' );
    is( $deltas1{days}, 0, 'delta_days is 0' );
    is( $deltas1{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas1{seconds}, 15901200, 'delta_seconds is 15901200' );
    is( $deltas1{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur1), $dt2 ), 0,
        'subtraction is reversible' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur1), $dt1 ), 0,
        'subtraction is doubly reversible' );

    my $dur2 = $dt1->subtract_datetime_absolute($dt2);

    my %deltas2 = $dur2->deltas;
    is( $deltas2{months}, 0, 'delta_months is 0' );
    is( $deltas2{days}, 0, 'delta_days is 0' );
    is( $deltas2{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas2{seconds}, -15901200, 'delta_seconds is -15901200' );
    is( $deltas2{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt2->clone->add_duration($dur2), $dt1 ), 0,
        'subtraction is reversible' );
    is( DateTime->compare( $dt1->clone->subtract_duration($dur2), $dt2 ), 0,
        'subtraction is doubly reversible' );
}

{
    my $dt1 = DateTime->new( year => 2003, month => 4, day => 6,
                             hour => 1, minute => 58,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 4, day => 6,
                             hour => 3, minute => 1,
                             time_zone => 'America/Chicago',
                           );

    my $dur = $dt2->subtract_datetime($dt1);

    my %deltas = $dur->deltas;
    is( $deltas{months}, 0, 'delta_months is 0' );
    is( $deltas{days}, 0, 'delta_days is 0' );
    is( $deltas{minutes}, 3, 'delta_minutes is 3' );
    is( $deltas{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur), $dt2 ), 0,
        'subtraction is reversible' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur), $dt1), 0,
        'subtraction is doubly reversible' );
}

{
    my $dt1 = DateTime->new( year => 2003, month => 4, day => 5,
                             hour => 1, minute => 58,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 4, day => 6,
                             hour => 3, minute => 1,
                             time_zone => 'America/Chicago',
                           );

    my $dur = $dt2->subtract_datetime($dt1);

    my %deltas = $dur->deltas;
    is( $deltas{months}, 0, 'delta_months is 0' );
    is( $deltas{days}, 1, 'delta_days is 1' );
    is( $deltas{minutes}, 3, 'delta_minutes is 3' );
    is( $deltas{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur), $dt2 ), 0,
        'dt1 + dur = dt2' );
    # this are two examples from the docs
    is( DateTime->compare( $dt2->clone->subtract_duration($dur),
                           $dt1->clone->add( hours => 1 ) ),
        0,
        'dt2 - dur != dt1 (not reversible)' );
    is( DateTime->compare( $dt2->clone->subtract_duration( $dur->clock_duration )
                               ->subtract_duration( $dur->calendar_duration ),
                           $dt1 ),
        0,
        'dt2 - dur->clock - dur->cal = dt1 (reversible when componentized)' );

    my $dur2 = $dt1->subtract_datetime($dt2);
    my %deltas2 = $dur2->deltas;
    is( $deltas2{months}, 0, 'delta_months is 0' );
    is( $deltas2{days}, -1, 'delta_days is 1' );
    is( $deltas2{minutes}, -3, 'delta_minutes is 3' );
    is( $deltas2{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas2{nanoseconds}, 0, 'delta_nanoseconds is 0' );
    is( $dt2->clone->add_duration($dur2)->datetime, '2003-04-05T02:58:00', 'dt2 + dur2 != dt1' );
    is( DateTime->compare( $dt2->clone->add_duration( $dur2->clock_duration )
                               ->add_duration( $dur2->calendar_duration ),
                           $dt1 ),
        0,
        'dt2 + dur2->clock + dur2->cal = dt1' );
    is( DateTime->compare( $dt1->clone->subtract_duration($dur2), $dt2 ), 0,
        'dt1 - dur2 = dt2' );

}

# These tests makes sure that days with DST changes are "normal" when
# they're the smaller operand
{
    my $dt1 = DateTime->new( year => 2003, month => 4, day => 6,
                             hour => 3, minute => 1,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 4, day => 7,
                             hour => 3, minute => 2,
                             time_zone => 'America/Chicago',
                           );

    my $dur = $dt2->subtract_datetime($dt1);

    my %deltas = $dur->deltas;
    is( $deltas{months}, 0, 'delta_months is 0' );
    is( $deltas{days}, 1, 'delta_days is 1' );
    is( $deltas{minutes}, 1, 'delta_minutes is 1' );
    is( $deltas{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    my $dur2 = $dt1->subtract_datetime($dt2);

    my %deltas2 = $dur2->deltas;
    is( $deltas2{months}, 0, 'delta_months is 0' );
    is( $deltas2{days}, -1, 'delta_days is -1' );
    is( $deltas2{minutes}, -1, 'delta_minutes is -1' );
    is( $deltas2{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas2{nanoseconds}, 0, 'delta_nanoseconds is 0' );

}

{
    my $dt1 = DateTime->new( year => 2003, month => 4, day => 5,
                             hour => 1, minute => 58,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 4, day => 7,
                             hour => 2, minute => 1,
                             time_zone => 'America/Chicago',
                           );

    my $dur = $dt2->subtract_datetime($dt1);

    my %deltas = $dur->deltas;
    is( $deltas{months}, 0, 'delta_months is 0' );
    is( $deltas{days}, 2, 'delta_days is 2' );
    is( $deltas{minutes}, 3, 'delta_minutes is 3' );
    is( $deltas{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur), $dt2 ), 0,
        'subtraction is reversible' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur), $dt1 ), 0,
        'subtraction is doubly reversible' );
}

# from example in docs
{
    my $dt1 = DateTime->new( year => 2003, month => 5, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 11, day => 6,
                             time_zone => 'America/Chicago',
                           );

    $dt1->set_time_zone('floating');
    $dt2->set_time_zone('floating');

    my $dur = $dt2->subtract_datetime($dt1);
    my %deltas = $dur->deltas;
    is( $deltas{months}, 6, 'delta_months is 6' );
    is( $deltas{days}, 0, 'delta_days is 0' );
    is( $deltas{minutes}, 0, 'delta_minutes is 0' );
    is( $deltas{seconds}, 0, 'delta_seconds is 0' );
    is( $deltas{nanoseconds}, 0, 'delta_nanoseconds is 0' );

    is( DateTime->compare( $dt1->clone->add_duration($dur), $dt2 ), 0,
        'subtraction is reversible from start point' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur), $dt1 ), 0,
        'subtraction is reversible from end point' );
}

{
    my $dt1 = DateTime->new( year => 2005, month => 8,
                             time_zone => 'Europe/London',
                           );

    my $dt2 = DateTime->new( year => 2005, month => 11,
                             time_zone => 'Europe/London',
                           );

    my $dur = $dt2->subtract_datetime($dt1);
    my %deltas = $dur->deltas;
    is( $deltas{months}, 3, '3 months between two local times over DST change' );
    is( $deltas{days}, 0, '0 days between two local times over DST change' );
    is( $deltas{minutes}, 0, '0 minutes between two local times over DST change' );
}

# same as previous but without hours overflow
{
    my $dt1 = DateTime->new( year => 2005, month => 8, hour => 12,
                             time_zone => 'Europe/London',
                           );

    my $dt2 = DateTime->new( year => 2005, month => 11, hour => 12,
                             time_zone => 'Europe/London',
                           );

    my $dur = $dt2->subtract_datetime($dt1);
    my %deltas = $dur->deltas;
    is( $deltas{months}, 3, '3 months between two local times over DST change' );
    is( $deltas{days}, 0, '0 days between two local times over DST change' );
    is( $deltas{minutes}, 0, '0 minutes between two local times over DST change' );
}

# another docs example
{
    my $dt2 = DateTime->new( year => 2003, month => 10, day => 26,
                             hour => 1,
                             time_zone => 'America/Chicago',
                           );

    my $dt1 = $dt2->clone->subtract( hours => 1 );

    my $dur = $dt2->subtract_datetime($dt1);

    my %deltas = $dur->deltas;
    is( $deltas{months}, 0, '0 months between two local times over DST change' );
    is( $deltas{days}, 0, '0 days between two local times over DST change' );
    is( $deltas{minutes}, 60, '60 minutes between two local times over DST change' );

    is( DateTime->compare( $dt1->clone->add_duration($dur), $dt2 ), 0,
        'subtraction is reversible' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur), $dt1 ), 0,
        'subtraction is doubly reversible' );
}

{
    my $dt1 = DateTime->new( year => 2003, month => 5, day => 6,
                             time_zone => 'America/New_York',
                           );

    my $dt2 = DateTime->new( year => 2003, month => 5, day => 6,
                             time_zone => 'America/Chicago',
                           );

    my $dur = $dt2->subtract_datetime($dt1);

    my %deltas = $dur->deltas;
    is( $deltas{months}, 0, '0 months between two local times over DST change' );
    is( $deltas{days}, 0, '0 days between two local times over DST change' );
    is( $deltas{minutes}, 60, '60 minutes between two local times over DST change' );

    is( DateTime->compare( $dt1->clone->add_duration($dur), $dt2 ), 0,
        'subtraction is reversible' );
    is( DateTime->compare( $dt2->clone->subtract_duration($dur), $dt1 ), 0,
        'subtraction is doubly reversible' );
}

# Fix a bug that occurred when the local time zone had DST and the two
# datetime objects were on the same day
{
    my $dt1 = DateTime->new( year => 2005, month => 4, day => 3,
                             hour => 7, minute => 0,
                             time_zone => 'America/New_York' );

    my $dt2 = DateTime->new( year => 2005, month => 4, day => 3,
                             hour => 8, minute => 0,
                             time_zone => 'America/New_York' );

    my $dur = $dt2->subtract_datetime($dt1);
    my ( $minutes, $seconds ) = $dur->in_units( 'minutes','seconds' );

    is( $minutes, 60, 'subtraction of two dates on a DST change date, minutes == 60' );
    is( $seconds, 0, 'subtraction of two dates on a DST change date, seconds == 0' );

    $dur = $dt1->subtract_datetime($dt1);
    ok( $dur->is_zero, 'dst change date (no dst) - itself, duration is zero' );
}

{
    my $dt1 = DateTime->new( year => 2005, month => 4, day => 3,
                             hour => 1, minute => 0,
                             time_zone => 'America/New_York' );

    my $dur = $dt1->subtract_datetime($dt1);
    ok( $dur->is_zero, 'dst change date (with dst) - itself, duration is zero' );
}

# This tests a bug where one of the datetimes is changing DST, and the
# other is not. In this case, no "adjustments" (aka hacks) are made in
# subtract_datetime, and it just gives the "UTC difference".
{
    # This is UTC-4
    my $dt1 = DateTime->new( year => 2009, month => 3, day => 9,
                             time_zone => 'America/New_York' );
    # This is UTC+8
    my $dt2 = DateTime->new( year => 2009, month => 3, day => 9,
                             time_zone => 'Asia/Hong_Kong' );

    my $dur = $dt1->subtract_datetime($dt2);

    is( $dur->delta_minutes, 720,
        'subtraction the day after a DST change in one zone, where the other datetime is in a different zone' );
}

{
    # This is UTC-5
    my $dt1 = DateTime->new( year => 2009, month => 3, day => 8,
                             hour => 1,
                             time_zone => 'America/New_York' );
    # This is UTC+8
    my $dt2 = DateTime->new( year => 2009, month => 3, day => 8,
                             hour => 1,
                             time_zone => 'Asia/Hong_Kong' );

    my $dur = $dt1->subtract_datetime($dt2);

    is( $dur->delta_minutes, 780,
        'subtraction the day of a DST change in one zone (before the change),'
        . ' where the other datetime is in a different zone' );
}


{
    # This is UTC-4
    my $dt1 = DateTime->new( year => 2009, month => 3, day => 8,
                             hour => 4,
                             time_zone => 'America/New_York' );
    # This is UTC+8
    my $dt2 = DateTime->new( year => 2009, month => 3, day => 8,
                             hour => 4,
                             time_zone => 'Asia/Hong_Kong' );

    my $dur = $dt1->subtract_datetime($dt2);

    is( $dur->delta_minutes, 720,
        'subtraction the day of a DST change in one zone (after the change),'
        . ' where the other datetime is in a different zone' );
}

