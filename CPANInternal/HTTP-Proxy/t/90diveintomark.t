# good place for web client tests:
# http://diveintomark.org/tests/client/http/

use strict;
my @url;
my $tests;

BEGIN {
    @url = (
        [ '200.xml', 200 ],
        [ '220.xml', 220 ],
        [ '320.xml', 320 ],
        [ '420.xml', 420 ],
        [ '520.xml', 520 ],
        [ '301.xml', 301,  200 ],
        [ '302.xml', 302,  200 ],
        [ '307.xml', 307,  200 ],
        [ '400.xml', 400 ],
        [ '403.xml', 403 ],
        [ '404.xml', 404 ],
        [ '410.xml', 410 ],
        [ '500.xml', 500 ],
        [ '300.xml', 300,  200 ],
        [ '200_basic_auth.xml',  401, 200 ], # these tests actually
        [ '200_digest_auth.xml', 401, 200 ], # do 401, then 401/200
        #['200_gzip.xml'],
        #['etag.xml'],
        #['lastmodified.xml'],
    );
    $tests += @$_ - 1 for @url;
}

use Test::More tests => $tests;
use HTTP::Proxy;
use HTTP::Request::Common;
use t::Utils;

my $base = 'http://diveintomark.org/tests/client/http';

SKIP:
{
    skip "$base is not available", $tests unless web_ok( $base );

    # $tests + 2, because of the duplicate 401
    my $proxy = HTTP::Proxy->new(
        port                    => 0,
        max_keep_alive_requests => $tests + 2,
        max_connections         => 1,
    );
    $proxy->init;

    # the auto-authenticating client
    {
        package MyUA;
        use base qw( LWP::UserAgent );

        my %credentials = (
            "Use test/basic" => [ "test", "basic" ],
            "DigestTest"     => [ "test", "digest" ],
        );

        sub get_basic_credentials {
            my($self, $realm, $uri) = @_;
            return @{$credentials{$realm}};
        }
    }

    my $ua = MyUA->new( keep_alive => 1 );
    $ua->proxy( http => $proxy->url );

    # fork the proxy
    my $pid = fork_proxy($proxy);

    # check all those pages
    for (@url) {
        my ( $doc, $status, $status2, $realm, $user, $pass ) = @$_;
        my $res = $ua->simple_request( GET "$base/$doc" );
        is( $res->code, $status, "$doc => $status" );

        # redirection
        if ( $res->is_redirect && $status2 ) {
            $res = $ua->simple_request( GET $res->header('Location') );
            is( $res->code, $status2, "$doc => $status2 (redirect)" );
        }

        # authentication
        if ( $res->code == 401 ) {
            # this request is actually two requests (401/200)
            $res = $ua->request( GET "$base/$doc" );
            is( $res->code, $status2, "$doc => $status2 (auth)" );
        }
    }

    # wait for the proxy
    wait;
}
