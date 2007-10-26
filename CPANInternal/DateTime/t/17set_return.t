#!/usr/bin/perl -w

use strict;

use Test::More tests => 7;

use DateTime;
use DateTime::Duration;

{
    my $dt = DateTime->now();
    my $du = DateTime::Duration->new( years => 1 );

    my $p;

    $p = $dt->set( year => 1882 );
    is( $p => $dt => "set() returns self" );

    $p = $dt->set_time_zone( 'Australia/Sydney' );
    is( $p => $dt => "set_time_zone() returns self" );

    $p = $dt->add_duration( $du );
    is( $p => $dt => "add_duration() returns self" );

    $p = $dt->add( years => 2 );
    is( $p => $dt => "add() returns self" );


    $p = $dt->subtract_duration( $du );
    is( $p => $dt => "subtract_duration() returns self" );

    $p = $dt->subtract( years => 3 );
    is( $p => $dt => "subtract() returns self" );

    $p = $dt->truncate( to => 'day' );
    is( $p => $dt => "truncate() returns self" );

}
