#!/usr/bin/perl -w

use strict;
use Test::More tests => 8;

use DateTime::Locale;

my $loc = DateTime::Locale->load('root');

is( $loc->default_date_format_length, 'medium', 'default date format length is medium' );
is( $loc->default_time_format_length, 'medium', 'default time format length is medium' );

is( $loc->default_date_format, "\%\{ce_year\}\ \%b\ \%\{day\}", 'check default date format' );
is( $loc->default_time_format, "\%H\:\%M\:\%S", 'check default date format' );

$loc->set_default_date_format_length('short');
$loc->set_default_time_format_length('short');

is( $loc->default_date_format_length, 'short', 'default date format length is short' );
is( $loc->default_time_format_length, 'short', 'default time format length is short' );

is( $loc->default_date_format, "\%y\/\%m\/\%d", 'check default date format' );
is( $loc->default_time_format, "\%H\:\%M", 'check default date format' );
