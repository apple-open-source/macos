#!/usr/bin/perl -w

use strict;

use Test::More tests => 26;

use DateTime;

# These tests should be the final word on dt addition involving a
# DST-changing time zone

# time addition is "wait X amount of time, then what does the clock
# say?"  this means it acts on the UTC components.
{
    my $dt = DateTime->new( year => 2003, month => 4, day => 6,
                            time_zone => 'America/Chicago',
                          );

    $dt->add( hours => 1 );
    is( $dt->datetime, '2003-04-06T01:00:00', 'add one hour to midnight, get 1 am' );

    eval { $dt->add( hours => 1 ) };
    is( $@, '', 'no error adding 1 hour just before DST leap forward' );
    is( $dt->datetime, '2003-04-06T03:00:00', 'add one hour to 1 am, get 3 am' );

    $dt->subtract( hours => 1 );
    is( $dt->datetime, '2003-04-06T01:00:00', 'subtract one hour from 3 am, get 1 am' );

    $dt->subtract( hours => 1 );
    is( $dt->datetime, '2003-04-06T00:00:00', 'subtract one hour from 1 am, get midnight' );
}

{
    my $dt = DateTime->new( year => 2003, month => 10, day => 26,
                            time_zone => 'America/Chicago',
                          );

    $dt->add( hours => 1 );
    is( $dt->datetime, '2003-10-26T01:00:00', 'add one hour to midnight, get 1 am' );

    $dt->add( hours => 1 );
    is( $dt->datetime, '2003-10-26T01:00:00', 'add one hour to 1 am, get 1 am (again)' );

    $dt->add( hours => 1 );
    is( $dt->datetime, '2003-10-26T02:00:00', 'add one hour to 1 am (2nd time), get 2 am' );

    $dt->subtract( hours => 1 );
    is( $dt->datetime, '2003-10-26T01:00:00', 'subtract 1 hour from 2 am, get 1 am' );

    $dt->subtract( hours => 1 );
    is( $dt->datetime, '2003-10-26T01:00:00', 'subtract 1 hour from 1 am, get 1 am (again)' );

    $dt->subtract( hours => 1 );
    is( $dt->datetime, '2003-10-26T00:00:00', 'subtract 1 hour from 1 am (2nd), get midnight' );
}

# date addition is "leave the clock alone, just change the date
# portion".  this means it acts on local components
{
    my $dt = DateTime->new( year => 2003, month => 4, day => 6,
                            time_zone => 'America/Chicago',
                          );

    $dt->add( days => 1 );
    is( $dt->datetime, '2003-04-07T00:00:00', 'add 1 day at midnight, same clock time' );

    $dt->add( months => 7 );
    is( $dt->datetime, '2003-11-07T00:00:00', 'add 7 months at midnight, same clock time' );

    $dt->subtract( months => 7 );
    is( $dt->datetime, '2003-04-07T00:00:00', 'subtract 7 months at midnight, same clock time' );

    $dt->subtract( days => 1 );
    is( $dt->datetime, '2003-04-06T00:00:00', 'subtract 1 day at midnight, same clock time' );
}

{
    my $dt = DateTime->new( year => 2003, month => 10, day => 26,
                            time_zone => 'America/Chicago',
                          );

    $dt->add( days => 1 );
    is( $dt->datetime, '2003-10-27T00:00:00', 'add 1 day at midnight, get midnight' );

    $dt->add( months => 7 );
    is( $dt->datetime, '2004-05-27T00:00:00', 'add 7 months at midnight, get midnight' );

    $dt->subtract( months => 7 );
    is( $dt->datetime, '2003-10-27T00:00:00', 'subtract 7 months at midnight, get midnight' );

    $dt->subtract( days => 1 );
    is( $dt->datetime, '2003-10-26T00:00:00', 'subtract 1 day at midnight, get midnight' );
}

# date and time addition in one call is still two separate operations.
# First we do date, then time.
{
    my $dt = DateTime->new( year => 2003, month => 4, day => 5,
                            time_zone => 'America/Chicago',
                          );

    $dt->add( days => 1, hours => 2 );
    is( $dt->datetime, '2003-04-06T03:00:00', 'add one day & 2 hours from midnight, get 3 am' );

    # !!! - not reversible this way - needs some good docs
    my $dt1 = $dt->clone->subtract( days => 1, hours => 2 );
    is( $dt1->datetime, '2003-04-05T01:00:00', 'subtract one day & 2 hours from 3 am, get 1 am' );

    # is reversible this way - also needs docs
    my $dt2 = $dt->clone->subtract( hours => 2 )->subtract( days => 1 );
    is( $dt2->datetime, '2003-04-05T00:00:00', 'subtract 2 hours and then one day from 3 am, get midnight' );
}

{
    my $dt = DateTime->new( year => 2003, month => 10, day => 25,
                            time_zone => 'America/Chicago',
                          );

    $dt->add( days => 1, hours => 2 );
    is( $dt->datetime, '2003-10-26T01:00:00', 'add one day & 2 hours from midnight, get 1 am' );

    my $dt1 = $dt->clone->subtract( days => 1, hours => 2 );
    is( $dt1->datetime, '2003-10-24T23:00:00', 'add one day & 2 hours from midnight, get 11 pm' );

    my $dt2 = $dt->clone->subtract( hours => 2 )->subtract( days => 1 );
    is( $dt2->datetime, '2003-10-25T00:00:00', 'subtract 2 hours and then one day from 3 am, get midnight' );
}

# an example from the docs
{
    my $dt = DateTime->new( year => 2003, month => 4, day => 5,
                            hour => 2,
                            time_zone => 'America/Chicago',
                          );

    $dt->add( hours => 24 );

    is( $dt->datetime, '2003-04-06T03:00:00',
        'datetime after adding 24 hours is 2003-04-06T03:00:00' );
}
