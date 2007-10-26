#!/usr/bin/perl -w

use strict;

use Test::More tests => 16;

use DateTime;

# Borrowed from Matt Sergeant's Time::Piece

# A table of MJD and components
my @mjd = ( '51603.524' =>
            { year   => 2000,
              month  => 2,
              day    => 29,
              hour   => 12,
              minute => 34,
              second => 56,
            },

            '40598.574' =>
            { year   => 1970,
              month  => 1,
              day    => 12,
              hour   => 13,
              minute => 46,
              second => 51,
            },

            '52411.140' =>
            { year   => 2002,
              month  => 5,
              day    => 17,
              hour   => 3,
              minute => 21,
              second => 43,
            },

            '53568.547' =>
            { year   => 2005,
              month  => 7,
              day    => 17,
              hour   => 13,
              minute => 8,
              second => 23,
            },

            '52295.218' =>
            { year   => 2002,
              month  => 1,
              day    => 21,
              hour   => 5,
              minute => 13,
              second => 20,
            },

            '52295.399' =>
            { year   => 2002,
              month  => 1,
              day    => 21,
              hour   => 9,
              minute => 35,
              second => 3,
            },

            # beginning of MJD
            '0.000' =>
            { year   => 1858,
              month  => 11,
              day    => 17,
              hour   => 0,
              minute => 0,
              second => 0,
            },

            # beginning of JD
            '-2400000.500' =>
            { year   => -4713,
              month  => 11,
              day    => 24,
              hour   => 12,
              minute => 0,
              second => 0,
            },
          );

while ( my ( $mjd, $comps ) = splice @mjd, 0, 2 )
{
    my $dt = DateTime->new( %$comps,
                            time_zone => 'UTC',
                          );

    is( sprintf( '%.3f', $dt->mjd ), $mjd, "MJD should be $mjd" );

    my $jd = sprintf( '%.3f', $mjd + 2_400_000.5 );
    is( sprintf( '%.3f', $dt->jd ), $jd, "JD should be $jd" );
}
