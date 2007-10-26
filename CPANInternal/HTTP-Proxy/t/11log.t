use Test::More;
use HTTP::Proxy qw(:log);
use strict;

my %mask = (
    CONNECT => CONNECT,
    DATA    => DATA,
    ENGINE  => ENGINE,
    ERROR   => ERROR,
    FILTERS => FILTERS,
    HEADERS => HEADERS,
    PROCESS => PROCESS,
    PROXY   => PROXY,
    SOCKET  => SOCKET,
    STATUS  => STATUS,
);

# try all combinations

my @tests = (
    [ NONE,                   qw( ERROR ) ],
    [ PROXY,                  qw( ERROR PROXY ) ],
    [ STATUS | SOCKET,        qw( ERROR SOCKET STATUS ) ],
    [ DATA | STATUS | SOCKET, qw( DATA ERROR SOCKET STATUS ) ],
    [   ALL, qw( CONNECT DATA ENGINE ERROR FILTERS
            HEADERS PROCESS PROXY SOCKET STATUS )
    ],
);

my $t;
$t += @$_ - 1 for @tests;
plan tests => $t;

# communicate with a pipe
pipe my ( $rh, $wh );
select( ( select($wh), $| = 1 )[0] );

# the proxy logs error to the pipe
my $proxy = HTTP::Proxy->new( logfh => $wh );

for (@tests) {
    my ( $mask, @msgs ) = @$_;
    $proxy->logmask($mask);
    $proxy->log( $mask{$_}, 'TEST', $_ ) for sort keys %mask;
    like( <$rh>, qr/TEST: $_$/, "mask $mask: $_ message" ) for @msgs;
}
close $wh;
print for <$rh>;

