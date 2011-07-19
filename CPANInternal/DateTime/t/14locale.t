#!/usr/bin/perl -w

use strict;

use Test::More tests => 8;

use DateTime;
use DateTime::Locale;

eval { DateTime->new( year => 100, locale => 'en_US' ) };
is( $@, '', 'make sure constructor accepts locale parameter' );

eval { DateTime->now( locale => 'en_US' ) };
is( $@, '', 'make sure constructor accepts locale parameter' );

eval { DateTime->today( locale => 'en_US' ) };
is( $@, '', 'make sure constructor accepts locale parameter' );

eval { DateTime->from_epoch( epoch => 1, locale => 'en_US' ) };
is( $@, '', 'make sure constructor accepts locale parameter' );

eval { DateTime->last_day_of_month( year => 100, month => 2, locale => 'en_US' ) };
is( $@, '', 'make sure constructor accepts locale parameter' );

{
    package DT::Object;
    sub utc_rd_values { ( 0, 0 ) }
}

eval { DateTime->from_object( object => (bless {}, 'DT::Object'), locale => 'en_US' ) };
is( $@, '', 'make sure constructor accepts locale parameter' );

eval { DateTime->new( year => 100, locale => DateTime::Locale->load('en_US') ) };
is( $@, '', 'make sure constructor accepts locale parameter as object' );

DateTime->DefaultLocale('it');
is( DateTime->now->locale->id, 'it', 'default locale should now be "it"' );
