#!/usr/bin/perl

# Copyright (C) 2005  Joshua Hoblitt
#
# $Id: 03_base_datetime.t,v 1.4 2007/04/11 01:11:42 jhoblitt Exp $

use strict;
use warnings;

use lib qw( ./lib );

use Test::More tests => 36;

use DateTime;
use DateTime::Format::ISO8601;

my @good_bases = (
    { year => 0 },
    { year => 1 },
    { year => 2500 },
    { year => 5000 },
    { year => 7500 },
    { year => 9998 },
    {
        year    => 9999,
        month   => 12,
        day     => 12,
        hour    => 23,
        minute  => 59,
        second  => 59,
        nanosecond => 999_999_999,
    },
);

my @test_bases = (
    [ "1945-09-02T09:04", '1945-W35' ],
    [ "1987-12-18T02:03", '1987-W51' ],
    [ "1988-05-05T04:05", '1988-W18' ],
    [ "1989-10-18T06:07", '1989-W42' ],
    [ "1991-03-21T08:09", '1991-W12' ],
);

my @bad_bases = (
    { year => -2 },
    { year => -1 },
    {
        year        => -1,
        month       => 12,
        day         => 31,
        hour        => 23,
        minute      => 59,
        second      => 59,
        nanosecond  => 999_999_999
    },
    { year => 10_000 },
    { year => 10_001 },
    { year => 10_002 },
);

foreach my $params ( @good_bases ) {
    my $dt = DateTime->new( %$params );

    {
        my $iso_parser = DateTime::Format::ISO8601->new(
            base_datetime => $dt,
        );
        isa_ok( $iso_parser, 'DateTime::Format::ISO8601' );
    }

    {
        my $iso_parser = DateTime::Format::ISO8601->new->set_base_datetime(
            object => $dt,
        );
        isa_ok( $iso_parser, 'DateTime::Format::ISO8601' );
    }
}

foreach ( @test_bases ) {
    my $iso_parser = DateTime::Format::ISO8601->new(
        base_datetime => DateTime::Format::ISO8601->parse_datetime( $_->[0] ),
    );

    {
        #tests...
        #_add_minute
        #_add_hour
        #_add_day
        #_add_month
        #_add_year

        #--ss,s --50,5
        my $dt = $iso_parser->parse_datetime( '--50,5' );
        is( $dt->strftime( "%Y-%m-%dT%H:%M" ), $_->[0] );
    }

    {
        #tests...
        #_add_week
        #_add_year

        #-W-D -W-5
        my $dt = $iso_parser->parse_datetime( '-W-5' );
        is( $dt->strftime( "%Y-W%V" ), $_->[1] );
    }
}

foreach my $params ( @bad_bases ) {
    my $dt = DateTime->new( %$params );

    eval {
        DateTime::Format::ISO8601->new(
            base_datetime => $dt,
        );
    };
    like( $@, qr/base_datetime must be (greater|less) then/ );

    eval {
        DateTime::Format::ISO8601->new->set_base_datetime(
            object => $dt,
        );
    };
    like( $@, qr/base_datetime must be (greater|less) then/ );
}
