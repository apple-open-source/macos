use strict;
use warnings;

use Test::More tests => 8;

use DateTime::Locale;

my $loc = DateTime::Locale->load('root');

is( $loc->default_date_format_length(), 'medium',
    'default date format length is medium' );
is( $loc->default_time_format_length(), 'medium',
    'default time format length is medium' );

is( $loc->date_format_default(), 'y MMM d',  'check default date format' );
is( $loc->time_format_default(), 'HH:mm:ss', 'check default date format' );

$loc->set_default_date_format_length('short');
$loc->set_default_time_format_length('short');

is( $loc->default_date_format_length(), 'short',
    'default date format length is short' );
is( $loc->default_time_format_length(), 'short',
    'default time format length is short' );

is( $loc->date_format_default(), 'yyyy-MM-dd', 'check default date format' );
is( $loc->time_format_default(), 'HH:mm',      'check default date format' );
