
use strict;
use warnings;

use Test::More;


plan skip_all => 'This test is only run for the module author'
    unless -d '.svn' || $ENV{IS_MAINTAINER};

eval "use Test::Pod::Coverage 1.04";
plan skip_all => "Test::Pod::Coverage 1.04 required for testing POD coverage"
    if $@;

my @modules = grep { ! /PP/ } all_modules();
plan tests => scalar @modules;

my %trustme =
    ( 'DateTime'           =>
      { trustme => [ qr/0$/, qr/^STORABLE/, 'utc_year',
                     'timegm',
                     # deprecated methods
                     'DefaultLanguage', 'era', 'language',
                   ] },
      'DateTime::Helpers'  =>
      { trustme => [ qr/./ ] },
      'DateTime::Infinite' =>
      { trustme => [ qr/^STORABLE/, qr/^set/, qr/^is_(?:in)?finite/,
                     'truncate' ] },
    );


for my $mod ( sort @modules )
{
    pod_coverage_ok( $mod, $trustme{$mod} || {}, $mod );
}
