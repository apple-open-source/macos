#!/usr/bin/perl -w
use strict;

use Test::More tests => 3;

use DateTime::Format::Builder;


SKIP: {
    my @mods;
    for (qw( HTTP Mail IBeat ))
    {
        my $mod = "DateTime::Format::$_";
        eval "require $mod";
        push @mods, $mod if $@;
    }
    skip "@mods not installed.", 3 if @mods;


    eval q|
        package DTFB::Quick;

        use DateTime::Format::Builder (
        parsers => { parse_datetime => [
            { Quick => 'DateTime::Format::HTTP' },
            { Quick => 'DateTime::Format::Mail' },
            { Quick => 'DateTime::Format::IBeat' },
        ]});
        
        1;
    |;
    die $@ if $@;

    my $get = sub { eval {
            DTFB::Quick
                ->parse_datetime($_[0])
                ->set_time_zone( 'UTC' )
                ->datetime
        } };


    for ( '@d19.07.03 @704', '20030719T155345Z' )
    {
        my $dt = $get->( $_ );
        is $dt, "2003-07-19T15:53:45", "Can parse [$_]";
    }

    for ( 'gibberish' )
    {
        my $dt = $get->( $_ );
        ok( !defined $dt, "Shouldn't parse [$_]" )
    }
}
