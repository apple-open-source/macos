#!/usr/bin/perl

# Copyright (C) 2005  Joshua Hoblitt
#
# $Id: 04_legacy_year.t,v 1.3 2007/04/11 01:11:42 jhoblitt Exp $

use strict;
use warnings;

use lib qw( ./lib );

use Test::More tests => 36;

use DateTime::Format::ISO8601;

{
    is( DateTime::Format::ISO8601->DefaultLegacyYear, 1 );
    my $iso_parser = DateTime::Format::ISO8601->new;
    is( $iso_parser->legacy_year, 1 );
}

foreach my $n ( 0, 1, undef ) {
    DateTime::Format::ISO8601->DefaultLegacyYear( $n );
    is( DateTime::Format::ISO8601->DefaultLegacyYear, $n );
    my $iso_parser = DateTime::Format::ISO8601->new;
    is( $iso_parser->legacy_year, $n );
}

foreach my $n ( -3 .. -1, 2 .. 4 ) {
    eval { DateTime::Format::ISO8601->DefaultLegacyYear( $n ) };
    like( $@, qr/did not pass the 'is 0, 1, or undef' callback/ );
}

# restore default legacy year behavior
DateTime::Format::ISO8601->DefaultLegacyYear( 1 );

foreach my $n ( 0, 1, undef ) {
    my $iso_parser = DateTime::Format::ISO8601->new( legacy_year => $n );
    isa_ok( $iso_parser, 'DateTime::Format::ISO8601' );
    is( $iso_parser->legacy_year, $n );

    {
        my $iso_parser = DateTime::Format::ISO8601->new->set_legacy_year( $n );
        is( $iso_parser->legacy_year, $n );
    }
}

foreach my $n ( -3 .. -1, 2 .. 4 ) {
    eval { DateTime::Format::ISO8601->new( legacy_year => $n ) };
    like( $@, qr/did not pass the 'is 0, 1, or undef' callback/ );

    eval { DateTime::Format::ISO8601->new->set_legacy_year( $n ) };
    like( $@, qr/did not pass the 'is 0, 1, or undef' callback/ );
}

{
    my $failed = 0;

    foreach my $year ( 0 .. 99 ) {
        $year *= 100; # [0, 9900], step 100
        my $iso_parser = DateTime::Format::ISO8601->new(
            legacy_year     => 0,
            base_datetime   => DateTime->new( year => $year ),
        );

        foreach my $tdy ( 0 .. 9 ) {
            $tdy *= 10; # [0, 90], step 10
            $tdy = sprintf( "%02d", $tdy );
            my $dt = $iso_parser->parse_datetime( "-$tdy" );
            $failed++ unless $dt->year eq sprintf(
                "%d", $iso_parser->base_datetime->strftime( "%C" ) . $tdy );
        }
    }

    is( $failed, 0, "parse_datetime() with a base_datetime" );
}
