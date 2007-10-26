#!/usr/bin/perl -w

use strict;

use Test::More tests => 25;

use DateTime;

foreach my $p ( { year => 2000, month => 13 },
                { year => 2000, month => 0 },
                { year => 2000, month => 12, day => 32 },
                { year => 2000, month => 12, day => 0 },
                { year => 2000, month => 12, day => 10, hour => -1 },
                { year => 2000, month => 12, day => 10, hour => 24 },
                { year => 2000, month => 12, day => 10, hour => 12, minute => -1 },
                { year => 2000, month => 12, day => 10, hour => 12, minute => 60 },
                { year => 2000, month => 12, day => 10, hour => 12, second => -1 },
                { year => 2000, month => 12, day => 10, hour => 12, second => 62 },
              )
{
    eval { DateTime->new(%$p) };
    like( $@, qr/did not pass/,
          "Parameters outside valid range should fail in call to new()" );

    eval { DateTime->new( year => 2000 )->set(%$p) };
    like( $@, qr/did not pass/,
          "Parameters outside valid range should fail in call to set()" );
}

{
    eval { DateTime->last_day_of_month( year => 2000, month => 13 ) };
    like( $@, qr/did not pass/,
          "Parameters outside valid range should fail in call to last_day_of_month()" );

    eval { DateTime->last_day_of_month( year => 2000, month => 0 ) };
    like( $@, qr/did not pass/,
          "Parameters outside valid range should fail in call to last_day_of_month()" );
}

{
    eval { DateTime->new( year => 2000, month => 4, day => 31 ) };
    like( $@, qr/valid day of month/i,
          "Day past last day of month should fail" );

    eval { DateTime->new( year => 2001, month => 2, day => 29 ) };
    like( $@, qr/valid day of month/i,
          "Day past last day of month should fail" );

    eval { DateTime->new( year => 2000, month => 2, day => 29 ) };
    ok( ! $@, "February 29 should be valid in leap years" );
}

