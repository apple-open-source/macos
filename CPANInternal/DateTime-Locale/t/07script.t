use strict;
use warnings;
use utf8;

use Test::More;

BEGIN {
    if ( $] <= 5.008 ) {
        plan skip_all => 'These tests require Perl 5.8.0+';
    }
    else {
        plan tests => 4;
    }
}

use DateTime::Locale;

my $loc = DateTime::Locale->load('zh_Hans_SG');

is( $loc->script,        'Simplified Han', 'check script' );
is( $loc->native_script, '简体中文',   'check native_script' );
is( $loc->script_id,     'Hans',           'check script_id' );

is( $loc->territory_id, 'SG', 'check territory_id' );
