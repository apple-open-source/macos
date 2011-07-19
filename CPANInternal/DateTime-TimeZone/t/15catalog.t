use strict;
use warnings;

use File::Spec;
use Test::More;

use DateTime::TimeZone;


plan tests => 31;

{
    my @all = DateTime::TimeZone::all_names();
    ok( scalar @all > 50, 'there are more than 50 names in the catalog' );
    ok( ( grep { $_ eq 'America/Chicago' } @all ),
        'America/Chicago is in the list of all names' );

    my $all = DateTime::TimeZone::all_names();
    ok( ref $all, 'all_names() returns ref in scalar context' );
}

{
    my @cats = DateTime::TimeZone::categories();
    my %cats = map { $_ => 1 } @cats;
    for my $c ( qw( Africa
                    America
                    Antarctica
                    Asia
                    Atlantic
                    Australia
                    Europe
                    Indian
                    Pacific
                  ) )
    {
        ok( $cats{$c}, "$c is in categories list" );
    }

    my $cats = DateTime::TimeZone::categories();
    ok( ref $cats, 'categories() returns ref in scalar context' );
}

{
    my %links = DateTime::TimeZone::links();
    is( $links{Israel}, 'Asia/Jerusalem', 'Israel links to Asia/Jerusalem' );
    is( $links{UCT}, 'UTC', 'UCT links to UTC' );

    my $links = DateTime::TimeZone::links();
    ok( ref $links, 'links() returns ref in scalar context' );
}


{
    my @names = DateTime::TimeZone::names_in_category('America');
    my %names = map { $_ => 1 } @names;
    for my $n ( qw( Chicago Adak ) )
    {
        ok( exists $names{$n}, "$n is in America category" );
    }

    my $names = DateTime::TimeZone::names_in_category('America');
    ok( ref $names, 'names_in_category() returns ref in scalar context' );
}

{
    my @names = DateTime::TimeZone->names_in_category('America');
    my %names = map { $_ => 1 } @names;
    for my $n ( qw( Chicago Adak ) )
    {
        ok( exists $names{$n}, "$n is in America category (names_in_category() called as class method)" );
    }
}

{
    my @countries = DateTime::TimeZone::countries();
    my %countries = map { $_ => 1 } @countries;
    for my $c ( qw( jp us ) )
    {
        ok( exists $countries{$c}, "$c is in the list of countries" );
    }
}

{
    my @zones = DateTime::TimeZone::names_in_country('jp');
    is( @zones, 1, 'one zone for Japan' );
    is( $zones[0], 'Asia/Tokyo', 'zone for Japan is Asia/Tokyo' );
}

{
    my @zones = DateTime::TimeZone::names_in_country('JP');
    is( @zones, 1, 'one zone for Japan' );
    is( $zones[0], 'Asia/Tokyo', 'zone for Japan is Asia/Tokyo (uc country code)' );
}

{
    my @zones = DateTime::TimeZone->names_in_country('cl');
    is( @zones, 2, 'two zones for Chile' );
    is_deeply( [ sort @zones ],
               [ 'America/Santiago', 'Pacific/Easter' ],
               'zones for Chile are America/Santiago and Pacific/Easter' );
}

{
    my @zones = DateTime::TimeZone::names_in_country('us');
    is( $zones[0], 'America/New_York',
        'First timezone by country in US is America/New_York' );
}

{
    my $zones = DateTime::TimeZone::names_in_country('us');
    is( $zones->[0], 'America/New_York',
        'First timezone by country in US is America/New_York - scalar context' );
}
