use strict;
use warnings;

use Test::Exception;
use Test::More tests => 8;

use DateTime;

throws_ok { DateTime->new( year => 10.5 ) } qr/is an integer/,
    'year must be an integer';
throws_ok { DateTime->new( year => -10.5 ) } qr/is an integer/,
    'year must be an integer';

throws_ok { DateTime->new( year => 10, month => 2.5 ) } qr/an integer/,
    'month must be an integer';

throws_ok { DateTime->new( year => 10, month => 2, day => 12.4 ) }
qr/an integer/, 'day must be an integer';

throws_ok { DateTime->new( year => 10, month => 2, day => 12, hour => 4.1 ) }
qr/an integer/, 'hour must be an integer';

throws_ok {
    DateTime->new(
        year   => 10,
        month  => 2,
        day    => 12,
        hour   => 4,
        minute => 12.2
    );
}
qr/an integer/, 'minute must be an integer';

throws_ok {
    DateTime->new(
        year   => 10,
        month  => 2,
        day    => 12,
        hour   => 4,
        minute => 12,
        second => 51.8
    );
}
qr/an integer/, 'second must be an integer';

throws_ok {
    DateTime->new(
        year       => 10,
        month      => 2,
        day        => 12,
        hour       => 4,
        minute     => 12,
        second     => 51,
        nanosecond => 124512.12412
    );
}
qr/positive integer/, 'nanosecond must be an integer';
