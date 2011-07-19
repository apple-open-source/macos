#!perl -w

# t/004_locale_defaults.t - check module dates in various formats

use Test::More tests => 60;
use DateTime::Format::Strptime;
use DateTime;
use DateTime::TimeZone;
use DateTime::Locale;

my $object = DateTime::Format::Strptime->new(
    pattern    => '%c',
    diagnostic => 0,
    on_error   => sub { warn @_ },
);

my @tests = (

    # Australian English
    [ 'en_AU', '%x', '31/12/98' ],
    [ 'en_AU', '%X', '13:34:56' ],
    [ 'en_AU', '%c', 'Thu 31 Dec 1998 13:34:56 AEDT' ],

    # US English
    [ 'en_US', '%x', '12/31/1998' ],
    [ 'en_US', '%X', '01:34:56 PM' ],
    [ 'en_US', '%c', 'Thu 31 Dec 1998 01:34:56 PM MST' ],

    # UK English
    [ 'en_GB', '%x', '31/12/98' ],
    [ 'en_GB', '%X', '13:34:56' ],
    [ 'en_GB', '%c', 'Thu 31 Dec 1998 13:34:56 GMT' ],

    # French French
    [ 'fr_FR', '%x', '31/12/1998' ],
    [ 'fr_FR', '%X', '13:34:56' ],
    [ 'fr_FR', '%c', 'jeu. 31 Déc 1998 13:34:56 CEST' ],

    # French Generic - inherits from root locale for glibc formats
    [ 'fr', '%x', '12/31/98' ],
    [ 'fr', '%X', '13:34:56' ],
    [ 'fr', '%c', 'jeu. Déc 31 13:34:56 1998' ],
);

foreach (@tests) {
    my ( $locale, $pattern, $data ) = @$_;
    $object->locale($locale);
    $object->pattern($pattern);
    my $datetime = $object->parse_datetime($data);

    unless ($datetime) {
        fail("Could not parse $data with $pattern for $locale") for 1..3;
        next;
    }

    if ( $pattern eq '%x' or $pattern eq '%c' ) {
        is( $datetime->year,  1998, $locale . ' : ' . $pattern . ' : year' );
        is( $datetime->month, 12,   $locale . ' : ' . $pattern . ' : month' );
        is( $datetime->day,   31,   $locale . ' : ' . $pattern . ' : day' );
    }
    if ( $pattern eq '%X' or $pattern eq '%c' ) {
        is( $datetime->hour,   13, $locale . ' : ' . $pattern . ' : hour' );
        is( $datetime->minute, 34, $locale . ' : ' . $pattern . ' : minute' );
        is( $datetime->second, 56, $locale . ' : ' . $pattern . ' : second' );
    }
}

