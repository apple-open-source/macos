#!/usr/bin/perl -w

use strict;

use File::Spec;
use Test::More;

use DateTime::TimeZoneCatalog;


plan tests => 21;

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
    is( $links{UCT}, 'Etc/UCT', 'UCT links to Etc/UCT' );

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
