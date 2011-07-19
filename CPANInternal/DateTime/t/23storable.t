#!/usr/bin/perl -w

use strict;

use Test::More;

use DateTime;

if ( eval { require Storable; 1 } )
{
    plan tests => 21;
}
else
{
    plan skip_all => 'Cannot load Storable';
}

{
    my @dt = ( DateTime->new( year => 1950,
                              hour => 1,
                              nanosecond => 1,
                              time_zone => 'America/Chicago',
                              language => 'German'
                            ),
               DateTime::Infinite::Past->new,
               DateTime::Infinite::Future->new,
             );

    foreach my $dt (@dt)
    {
        my $copy   = Storable::thaw( Storable::nfreeze($dt) );

        is( $copy->time_zone->name, $dt->time_zone->name,
            'Storable freeze/thaw preserves tz' );

        is( ref $copy->locale, ref $dt->locale,
            'Storable freeze/thaw preserves locale' );

        is( $copy->year, $dt->year,
            'Storable freeze/thaw preserves rd values' );

        is( $copy->hour, $dt->hour,
            'Storable freeze/thaw preserves rd values' );

        is( $copy->nanosecond, $dt->nanosecond,
            'Storable freeze/thaw preserves rd values' );
    }
}

{
    my $dt1 = DateTime->now( locale => 'en_US' );
    my $dt2 = Storable::dclone($dt1);
    my $dt3 = Storable::thaw( Storable::nfreeze($dt2) );

    is( $dt1->iso8601, $dt2->iso8601,
        'dclone produces date equal to original' );
    is( $dt2->iso8601, $dt3->iso8601,
        'explicit freeze and thaw produces date equal to original' );

    is( $dt1->locale->id, 'en_US',
        'check id of original locale object' );
    is( $dt2->locale->id, 'en_US',
        'check locale id after dclone' );
    is( $dt3->locale->id, 'en_US',
        'check locale id after explicit freeze/thaw' );
}

{
    my $has_ical = eval { require DateTime::Format::ICal; 1 };

 SKIP:
    {
        skip 'This test needs DateTime::Format::ICal', 1
            unless $has_ical;

        my $dt = DateTime->new( year      => 2004,
                                formatter => 'DateTime::Format::ICal',
                              );

        my $copy = Storable::thaw( Storable::nfreeze($dt) );

        is( $dt->formatter, $copy->formatter,
            'Storable freeze/thaw preserves formatter' );
    }
}
