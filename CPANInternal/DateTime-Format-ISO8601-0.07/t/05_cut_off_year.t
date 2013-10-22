#!/usr/bin/perl

# Copyright (C) 2005  Joshua Hoblitt
#
# $Id: 05_cut_off_year.t,v 1.3 2007/04/11 01:11:42 jhoblitt Exp $

use strict;
use warnings;

use lib qw( ./lib );

use Test::More tests => 24;

use DateTime::Format::ISO8601;

# DefaultCutOffYear()

{
    is( DateTime::Format::ISO8601->DefaultCutOffYear, 49,
        "class default DefaultCutOffYear()" );
    is( DateTime::Format::ISO8601->new->cut_off_year, 49,
        "object default DefaultCutOffYear()" );
}

{
    my $failed = 0;

    foreach my $n ( 0 .. 99 ) {
        DateTime::Format::ISO8601->DefaultCutOffYear( $n );

        $failed++ unless DateTime::Format::ISO8601->DefaultCutOffYear == $n;
        $failed++ unless DateTime::Format::ISO8601->new->cut_off_year == $n;
    }

    is( $failed, 0, "set default DefaultCutOffYear()" );
}

foreach my $n ( -3 .. -1, 100 .. 102 ) {
    eval { DateTime::Format::ISO8601->DefaultCutOffYear( $n ) };
    like( $@, qr/did not pass the 'is between 0 and 99' callback/ );
}

# restore default cut off year behavior
DateTime::Format::ISO8601->DefaultCutOffYear( 49 );

# set_cut_off_year()

{
    my $failed = 0;

    foreach my $n ( 0 .. 99 ) {
        {
            my $iso_parser = DateTime::Format::ISO8601->new( cut_off_year => $n );
            $failed++ unless UNIVERSAL::isa( $iso_parser, 'DateTime::Format::ISO8601' );
            $failed++ unless $iso_parser->cut_off_year == $n;
        }

        {
            my $iso_parser = DateTime::Format::ISO8601->new->set_cut_off_year( $n );
            $failed++ unless $iso_parser->cut_off_year == $n;
        }
    }

    is( $failed, 0, "set_cut_off_year()" );
}

foreach my $n ( -3 .. -1, 100 .. 102 ) {
    eval { DateTime::Format::ISO8601->new( cut_off_year => $n ) };
    like( $@, qr/did not pass the 'is between 0 and 99' callback/,
        "cut_off_year value out of range" );

    eval { DateTime::Format::ISO8601->new->set_cut_off_year( $n ) };
    like( $@, qr/did not pass the 'is between 0 and 99' callback/,
        "set_cut_off_year() value out of range" );
}

# parse_datetime() as a class method

{
    my $failed = 0;

    foreach my $n ( 0 .. 99 ) {
        DateTime::Format::ISO8601->DefaultCutOffYear( $n );

        foreach my $i ( 0 .. DateTime::Format::ISO8601->DefaultCutOffYear ) {
            my $tdy = sprintf( "%02d", $i );
            my $dt = DateTime::Format::ISO8601->parse_datetime( "-$tdy" );
            $failed++ unless ( $dt->year eq "20$tdy" );
                
        }

        foreach my $i ( ( DateTime::Format::ISO8601->DefaultCutOffYear + 1 ) .. 99 ) {
            my $tdy = sprintf( "%02d", $i );
            my $dt = DateTime::Format::ISO8601->parse_datetime( "-$tdy" );
            $failed++ unless ( $dt->year eq "19$tdy" );
        }
    }

    is( $failed, 0, "parse_datetime() as a class method" );
}

# parse_datetime() as an object method

{
    my $failed = 0;

    foreach my $n ( 0 .. 99 ) {
        my $iso_parser = DateTime::Format::ISO8601->new( cut_off_year => $n );

        foreach my $i ( 0 .. $iso_parser->cut_off_year ) {
            my $tdy = sprintf( "%02d", $i );
            my $dt = $iso_parser->parse_datetime( "-$tdy" );
            $failed++ unless ( $dt->year eq "20$tdy" );
        }

        foreach my $i ( ( $iso_parser->cut_off_year + 1 ) .. 99 ) {
            my $tdy = sprintf( "%02d", $i );
            my $dt = $iso_parser->parse_datetime( "-$tdy" );
            $failed++ unless ( $dt->year eq "19$tdy" );
        }
    }
    
    is( $failed, 0, "parse_datetime() as an object method" );
}
