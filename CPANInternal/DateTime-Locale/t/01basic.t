#!/usr/bin/perl -w

BEGIN
{
    return unless $] >= 5.006;

    require utf8; import utf8;
}

use strict;
use Test::More;

use DateTime::Locale;

my @locale_ids   = DateTime::Locale->ids;
my %locale_names = map { $_ => 1 } DateTime::Locale->names;
my %locale_ids   = map { $_ => 1 } DateTime::Locale->ids;

eval { require DateTime };
my $has_dt = $@ ? 0 : 1;

my $dt = DateTime->new( year => 2000, month => 1, day => 1, time_zone => "UTC" )
    if $has_dt;

my $tests_per_locale = $has_dt ? 16 : 12;

plan tests =>
    5    # starting
    + ( @locale_ids * $tests_per_locale ) # test each local
    + 55 # check_en_GB
    + 11 # check_es_ES
    + 5  # check_en_US_POSIX
    + 9  # check_DT_Lang
    ;

ok( @locale_ids >= 240,     "Coverage looks complete" );
ok( $locale_names{English}, "Locale name 'English' found" );
ok( $locale_ids{ar_JO},     "Locale id 'ar_JO' found" );

eval { DateTime::Locale->load('Does not exist') };
like( $@, qr/invalid/i, 'invalid locale name/id to load() causes an error' );

{
    # this type of locale id should work
    my $l = DateTime::Locale->load('en_US.LATIN-1');
    is( $l->id, 'en_US', 'id is en_US' );
}

for my $locale_id ( @locale_ids )
{
    my $locale;

    eval
    {
        $locale = DateTime::Locale->load($locale_id);
    };

    isa_ok( $locale, "DateTime::Locale::Base" );

    $@ and warn("$@\nSkipping tests for failed locale: '$locale_id'"), next;

    ok( $locale_ids{ $locale->id },  "'$locale_id':  Has a valid locale id" );

    $locale_id = $locale_id . "(" . $locale->id . ")";

    ok( length $locale->name,        "'$locale_id':  Has a locale name"        );
    ok( length $locale->native_name, "'$locale_id':  Has a native locale name" );

    check_array($locale_id, $locale, "month_names",         "month_name",         "month", 12);
    check_array($locale_id, $locale, "month_abbreviations", "month_abbreviation", "month", 12);

    check_array($locale_id, $locale, "day_names",           "day_name",           "day",   7 );
    check_array($locale_id, $locale, "day_abbreviations",   "day_abbreviation",   "day",   7 );

    check_formats($locale_id, $locale, "date_formats",        "date_format");
    check_formats($locale_id, $locale, "time_formats",        "time_format");
}

check_en_GB();
check_es_ES();
check_en_US_POSIX();
check_DT_Lang();

# does 2 tests
sub check_array
{
    my ($locale_id, $locale, $array_func, $item_func, $dt_component, $count) = @_;

    my %unique = map { $_ => 1 } @{ $locale->$array_func() };

    is( keys %unique, $count, "'$locale_id': '$array_func' contains $count unique items" );

    if ($has_dt)
    {
        for my $i ( 1..$count )
        {
            $dt->set($dt_component => $i);

            delete $unique{ $locale->$item_func($dt) };
        }

        is( keys %unique, 0,
            "'$locale_id':  Data returned by '$array_func' and '$item_func match' matches" );
    }
}

# does 2 tests
sub check_formats
{
    my ($locale_id, $locale, $hash_func, $item_func) = @_;

    my %unique = map { $_ => 1 } values %{ $locale->$hash_func() };

    ok( keys %unique >= 1, "'$locale_id': '$hash_func' contains at least 1 unique item" );

    foreach my $format ( qw( full long medium short ) )
    {
        my $method = "${format}_$item_func";

        my $val = $locale->$method();

        if ( defined $val )
        {
            delete $unique{$val};
        }
        else
        {
            Test::More::diag( "$locale_id returned undef for $method()" );
        }
    }

    is( keys %unique, 0,
        "'$locale_id':  Data returned by '$hash_func' and '$item_func patterns' matches" );
}

# does 46 tests
sub check_en_GB
{
    my $locale = DateTime::Locale->load("en_GB");

    is( $locale->day_names->[0], "Monday",    "Check names: Monday" );
    is( $locale->day_names->[1], "Tuesday",   "Check names: Tuesday" );
    is( $locale->day_names->[2], "Wednesday", "Check names: Wednesday" );
    is( $locale->day_names->[3], "Thursday",  "Check names: Thursday" );
    is( $locale->day_names->[4], "Friday",    "Check names: Friday" );
    is( $locale->day_names->[5], "Saturday",  "Check names: Saturday" );
    is( $locale->day_names->[6], "Sunday",    "Check names: Sunday" );

    is( $locale->day_abbreviations->[0], "Mon", "Check names: Mon" );
    is( $locale->day_abbreviations->[1], "Tue", "Check names: Tue" );
    is( $locale->day_abbreviations->[2], "Wed", "Check names: Wed" );
    is( $locale->day_abbreviations->[3], "Thu", "Check names: Thu" );
    is( $locale->day_abbreviations->[4], "Fri", "Check names: Fri" );
    is( $locale->day_abbreviations->[5], "Sat", "Check names: Sat" );
    is( $locale->day_abbreviations->[6], "Sun", "Check names: Sun" );

    is( $locale->month_names->[0] , "January",   "Check names: January"  );
    is( $locale->month_names->[1] , "February",  "Check names: February" );
    is( $locale->month_names->[2] , "March",     "Check names: March"    );
    is( $locale->month_names->[3] , "April",     "Check names: April"    );
    is( $locale->month_names->[4] , "May",       "Check names: May"      );
    is( $locale->month_names->[5] , "June",      "Check names: June"     );
    is( $locale->month_names->[6] , "July",      "Check names: July"     );
    is( $locale->month_names->[7] , "August",    "Check names: August"   );
    is( $locale->month_names->[8] , "September", "Check names: September" );
    is( $locale->month_names->[9] , "October",   "Check names: October"  );
    is( $locale->month_names->[10], "November",  "Check names: November" );
    is( $locale->month_names->[11], "December",  "Check names: December" );

    is( $locale->month_abbreviations->[0] , "Jan", "Check names: Jan" );
    is( $locale->month_abbreviations->[1] , "Feb", "Check names: Feb" );
    is( $locale->month_abbreviations->[2] , "Mar", "Check names: Mar" );
    is( $locale->month_abbreviations->[3] , "Apr", "Check names: Apr" );
    is( $locale->month_abbreviations->[4] , "May", "Check names: May" );
    is( $locale->month_abbreviations->[5] , "Jun", "Check names: Jun" );
    is( $locale->month_abbreviations->[6] , "Jul", "Check names: Jul" );
    is( $locale->month_abbreviations->[7] , "Aug", "Check names: Aug" );
    is( $locale->month_abbreviations->[8] , "Sep", "Check names: Sep" );
    is( $locale->month_abbreviations->[9] , "Oct", "Check names: Oct" );
    is( $locale->month_abbreviations->[10], "Nov", "Check names: Nov" );
    is( $locale->month_abbreviations->[11], "Dec", "Check names: Dec" );

    is( $locale->eras->[0]       , "BC", "Check names: BC" );
    is( $locale->eras->[1]       , "AD", "Check names: AD" );

    is( $locale->am_pms->[0]     , "AM", "Check names: AM" );
    is( $locale->am_pms->[1]     , "PM", "Check names: PM" );

    is( $locale->name, "English United Kingdom", 'name()' );
    is( $locale->native_name, "English United Kingdom", 'native_name()' );
    is( $locale->language, "English", 'language()' );
    is( $locale->native_language, "English", 'native_language()' );
    is( $locale->territory, "United Kingdom", 'territory()' );
    is( $locale->native_territory, "United Kingdom", 'native_territory()' );
    is( $locale->variant, undef, 'variant()' );
    is( $locale->native_variant, undef, 'native_variant()' );

    is( $locale->language_id, 'en', 'language_id()' );
    is( $locale->territory_id, 'GB', 'territory_id()' );
    is( $locale->variant_id, undef, 'variant_id()' );

    is( $locale->default_datetime_format, "\%\{day\}\ \%b\ \%\{ce_year\} \%H\:\%M\:\%S",
        'check default datetime format' );

    is( $locale->date_parts_order, 'dmy', 'date_parts_order' );
}

sub check_es_ES
{
    my $locale = DateTime::Locale->load('es_ES');

    is( $locale->name, 'Spanish Spain', 'name()' );
    is( $locale->native_name, 'español España', 'native_name()' );
    is( $locale->language, 'Spanish', 'language()' );
    is( $locale->native_language, 'español', 'native_language()' );
    is( $locale->territory, 'Spain', 'territory()' );
    is( $locale->native_territory, 'España', 'native_territory()' );
    is( $locale->variant, undef, 'variant()' );
    is( $locale->native_variant, undef, 'native_variant()' );

    is( $locale->language_id, 'es', 'language_id()' );
    is( $locale->territory_id, 'ES', 'territory_id()' );
    is( $locale->variant_id, undef, 'variant_id()' );
}

sub check_en_US_POSIX
{
    my $locale = DateTime::Locale->load('en_US_POSIX');

    is( $locale->variant, 'Posix', 'variant()' );
    is( $locale->native_variant, 'Posix', 'native_variant()' );

    is( $locale->language_id, 'en', 'language_id()' );
    is( $locale->territory_id, 'US', 'territory_id()' );
    is( $locale->variant_id, 'POSIX', 'variant_id()' );
}

sub check_DT_Lang
{
    foreach my $old ( qw ( Austrian TigrinyaEthiopian TigrinyaEritrean
                           Brazilian Portuguese
                           Afar Sidama Tigre ) )
    {
        ok( DateTime::Locale->load($old), "backwards compatibility for $old" );
    }


    foreach my $old ( qw ( Gedeo ) )
    {
      SKIP:
        {
            skip 'No ICU XML data for some African languages included in DT::Language', 1
                unless $locale_names{$old};

            ok( DateTime::Locale->load($old), "backwards compatibility for $old" );
        }
    }
}

