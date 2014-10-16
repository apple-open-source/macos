#!perl -w

# t/002_basic.t - check module dates in various formats

use Test::More 0.88;
use DateTime::Format::Strptime;
use DateTime;
use DateTime::TimeZone;

my $object = DateTime::Format::Strptime->new(
    pattern => '%D',

    #	time_zone => 'Australia/Melbourne',
    diagnostic => 1,
    on_error   => 'croak',
);

my @tests = (

    # Simple dates
    [ '%Y-%m-%d',          '1998-12-31' ],
    [ '%y-%m-%d',          '98-12-31' ],
    [ '%Y years, %j days', '1998 years, 312 days' ],
    [ '%b %d, %Y',         'Jan 24, 2003' ],
    [ '%B %d, %Y',         'January 24, 2003' ],

    # Simple times
    [ '%H:%M:%S',    '23:45:56' ],
    [ '%l:%M:%S %p', '11:34:56 PM' ],

    # With Nanoseconds
    [ '%H:%M:%S.%N',  '23:45:56.123456789' ],
    [ '%H:%M:%S.%6N', '23:45:56.123456' ],
    [ '%H:%M:%S.%3N', '23:45:56.123' ],

    # Complex dates
    [ '%Y;%j = %Y-%m-%d',      '2003;056 = 2003-02-25' ],
    [ q|%d %b '%y = %Y-%m-%d|, q|25 Feb '03 = 2003-02-25| ],

    # Leading spaces
    [ '%e-%b-%Y %T %z', '13-Jun-2010 09:20:47 -0400' ],
    [ '%e-%b-%Y %T %z', ' 3-Jun-2010 09:20:47 -0400' ],
);

foreach (@tests) {
    my ( $pattern, $data, $expect ) = @$_;
    $expect ||= $data;
    $object->pattern($pattern);
    is(
        $object->format_datetime( $object->parse_datetime($data) ), $expect,
        $pattern
    );
}

SKIP: {
    skip
        "You don't have the latest DateTime. Older version have a bug whereby 12am and 12pm are shown as 0am and 0pm. You should upgrade.",
        1
        unless $DateTime::VERSION >= 0.11;

    $object->pattern('%l:%M:%S %p');
    is(
        $object->format_datetime( $object->parse_datetime('12:34:56 AM') ),
        '12:34:56 AM', '%l:%M:%S %p'
    );
}

# Timezones
SKIP: {
    skip
        "You don't have the latest DateTime::TimeZone. Older versions don't display all time zone information. You should upgrade.",
        3
        unless $DateTime::TimeZone::VERSION >= 0.13;

    $object->pattern('%H:%M:%S %z');
    is(
        $object->format_datetime( $object->parse_datetime('23:45:56 +1000') ),
        '23:45:56 +1000', '%H:%M:%S %z'
    );

    $object->pattern('%H:%M:%S %Z');
    is(
        $object->format_datetime( $object->parse_datetime('23:45:56 AEST') ),
        '23:45:56 +1000', '%H:%M:%S %Z'
    );

    $object->pattern('%H:%M:%S %z %Z');
    is(
        $object->format_datetime(
            $object->parse_datetime('23:45:56 +1000 AEST')
        ),
        '23:45:56 +1000 +1000',
        '%H:%M:%S %z %Z'
    );
}

$object->time_zone('Australia/Perth');
$object->pattern('%Y %H:%M:%S %Z');
is(
    $object->format_datetime( $object->parse_datetime('2003 23:45:56 MDT') ),
    '2003 13:45:56 WST', $object->pattern
);

done_testing();
