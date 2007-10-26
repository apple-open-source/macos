#!/usr/bin/perl -w

use strict;

use Test::More;

BEGIN
{
    eval { require Storable };

    if ($@)
    {
        plan skip_all => 'These tests require the Storable mdoule';
    }
    else
    {
        plan tests => 2;
    }
}

use DateTime::Locale;

use Storable;

my $tz1 = DateTime::Locale->load( 'en_US' );
my $frozen = Storable::nfreeze($tz1);

ok( length $frozen < 2000,
    'the serialized tz object should not be immense' );

my $tz2 = Storable::thaw($frozen);

is( $tz2->id, 'en_US', 'thaw frozen locale object' );
