#!/usr/bin/perl -w

use strict;
use Test::More tests => 5;

use DateTime::Locale;
use DateTime::Locale::Alias::ISO639_2;

my $locale = DateTime::Locale->load('eng_US');

is( $locale->id, 'eng_US', 'variant()' );

is( $locale->name, 'English United States', 'name()' );
is( $locale->language, 'English', 'language()' );
is( $locale->territory, 'United States', 'territory()' );
is( $locale->variant, undef, 'variant()' );
