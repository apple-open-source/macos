use strict;
use warnings;

use Test::More;

BEGIN {
    plan skip_all => 'This test is only run for the module author'
        unless -d '.hg' || $ENV{IS_MAINTAINER};
}

use File::Find::Rule;
use Test::Pod::Coverage 1.04;

my $dir = -d 'blib' ? 'blib' : 'lib';

my @files = sort File::Find::Rule ->file->name('*.pm')
    ->not( File::Find::Rule->grep('This file is auto-generated') )->in($dir);

plan tests => scalar @files;

for my $file (@files) {
    ( my $mod = $file ) =~ s/^.+(DateTime.+)\.pm$/$1/;
    $mod =~ s{/}{::}g;

    my @trustme = qr/^STORABLE_/;
    if ( $mod eq 'DateTime::Locale::Base' ) {

        # This is mostly the backwards compatibility cruft.
        push @trustme, (
            map {qr/^\Q$_\E$/}
                qw( new

                month_name
                month_abbreviation
                month_narrow

                month_names
                month_abbreviations
                month_narrows

                day_name
                day_abbreviation
                day_narrow

                day_names
                day_abbreviations
                day_narrows

                quarter_name
                quarter_abbreviation
                quarter_narrow

                quarter_names
                quarter_abbreviations

                am_pm
                am_pms

                era_name
                era_abbreviation
                era_narrow

                era_names
                era_abbreviations

                era
                eras

                date_before_time
                date_parts_order

                full_date_format
                long_date_format
                medium_date_format
                short_date_format
                default_date_format

                full_time_format
                long_time_format
                medium_time_format
                short_time_format
                default_time_format

                full_datetime_format
                long_datetime_format
                medium_datetime_format
                short_datetime_format
                default_datetime_format
                )
        );
    }

    pod_coverage_ok( $mod, { trustme => \@trustme } );
}
