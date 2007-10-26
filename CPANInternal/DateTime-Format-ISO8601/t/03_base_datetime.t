use strict;

use Test::More tests => 7010;

use DateTime;
use DateTime::Format::ISO8601;

my @good_bases = qw(
    0000-01-01T00:00:00
    0001-01-01T00:00:00
    2500-03-03T06:15:15
    5000-05-05T12:30:30
    7500-09-09T18:45:45
    9998-12-12T23:59:59
    9999-12-12T23:59:59
);

my @test_bases = (
    [ "1945-09-02T09:04", '1945-W35' ],
    [ "1987-12-18T02:03", '1987-W51' ],
    [ "1988-05-05T04:05", '1988-W18' ],
    [ "1989-10-18T06:07", '1989-W42' ],
    [ "1991-03-21T08:09", '1991-W12' ],
);

foreach ( 0 .. 99 ) {
    #tests...
    #new
    #base_datetime
    #set_base_datetime

    my $base = $_ * 100;

    foreach ( 0 .. 9 ) {
        $_ *= 10;
        $_ += $base;

        my $dt = DateTime->new(
                    year    => $_,
                    month   => $_ % 12 || 1,
                    day     => $_ % 28 || 1,
                    hour    => $_ % 23,
                    minute  => $_ % 59,
                    second  => $_ % 59,
                );

        my $iso_parser = DateTime::Format::ISO8601->new(
            base_datetime => $dt,
        );

        isa_ok( $iso_parser, 'DateTime::Format::ISO8601' );
        is( $iso_parser->base_datetime->iso8601, $dt->iso8601 );

        $iso_parser->set_base_datetime(
            object => $dt,
        );
        is( $iso_parser->base_datetime->iso8601, $dt->iso8601 );
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

foreach ( -99 .. -1, 100 .. 200 ) {
    my $base = $_ * 100;

    foreach ( 0 .. 9 ) {
        $_ *= 10;
        $_ += $base;

        my $dt = DateTime->new(
                    year    => $_,
                    month   => $_ % 12 || 1,
                    day     => $_ % 28 || 1,
                    hour    => $_ % 23,
                    minute  => $_ % 59,
                    second  => $_ % 59,
                );

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
}
