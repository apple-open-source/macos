# good place for web client tests:
# http://diveintomark.org/tests/client/http/

use strict;
my @url;
my $tests;

BEGIN {
    @url = (
        map ( [ "$_" => 0 + $_ ], 200 .. 206, 300, 304, 306 ),
        map ( [ "$_" => 0 + $_, 200 ], 301 .. 303, 305, 307 ),
        map ( [ "$_" => 0 + $_ ], 400 .. 418, 500 .. 505 ),
    );
    $tests += @$_ - 1 for @url;
}

use Test::More;
use HTTP::Proxy;
use HTTP::Request::Common;
use t::Utils;

my $base = 'http://httpstat.us';

plan tests => $tests;

SKIP:
{
    skip "$base is not available", $tests unless web_ok($base);

    # $tests + 2, because of the duplicate 401
    my $proxy = HTTP::Proxy->new(
        port                    => 0,
        max_keep_alive_requests => $tests,
        max_connections         => 1,
    );
    $proxy->init;

    my $ua = LWP::UserAgent->new( keep_alive => 1 );
    $ua->proxy( http => $proxy->url );

    # fork the proxy
    my $pid = fork_proxy($proxy);

    # check all those pages
    for (@url) {
        my ( $doc, $status, $status2 ) = @$_;
        my $res = $ua->simple_request( GET "$base/$doc" );
        is( $res->code, $status, "$doc => $status " . $res->message );

        # redirection
        if ( $res->is_redirect && $status2 ) {
            $res = $ua->simple_request( GET $res->header('Location') );
            is( $res->code, $status2, "$doc => $status2 (redirect)" );
        }
    }

    # wait for the proxy
    wait;
}
