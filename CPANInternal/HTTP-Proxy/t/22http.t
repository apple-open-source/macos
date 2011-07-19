use strict;
use vars qw( @requests );
use Socket;

# here are all the requests the client will try
BEGIN {
    @requests = (

        # host, expected code, shouldn't resolve
        [ 'http://www.mongueurs.net/',      200 ],
        [ 'http://httpd.apache.org/docs',   301 ],
        [ 'http://www.google.com/testing/', 404 ],
        [ 'http://www.error.zzz/', '5..', 1 ],
    );
}

use Test::More tests => @requests + 1;
use t::Utils;
use LWP::UserAgent;
use HTTP::Proxy;

# we skip the tests if the network is not available

SKIP: {
    skip "Web does not seem to work", @requests + 1 unless web_ok();

    my $test = Test::Builder->new;

    # this is to work around tests in forked processes
    $test->use_numbers(0);
    $test->no_ending(1);

    my $proxy = HTTP::Proxy->new(
        port            => 0,
        max_connections => scalar @requests,
    );
    $proxy->init;    # required to access the url later

    # fork a HTTP proxy
    my $pid = fork_proxy(
        $proxy,
        sub {
            ok( $proxy->conn == @requests,
                "Served the correct number of requests" );
        }
    );

    # run a client
    my $ua = LWP::UserAgent->new;
    $ua->proxy( http => $proxy->url );

    for (@requests) {
        my ( $uri, $code, $dns_fail ) = @$_;
        $uri = URI->new($uri);
        $dns_fail &&= defined +( gethostbyname $uri->host )[4];

    SKIP: {
            if ($dns_fail) {

                # contact the proxy anyway
                $ua->simple_request(
                    HTTP::Request->new( GET => 'http://localhost/' ) );
                skip "Our DNS shouldn't resolve " . $uri->host, 1;
            }
            else {

                # the real test
                my $req = HTTP::Request->new( GET => $uri );
                my $rep = $ua->simple_request($req);
                like(
                    $rep->code, qr/^$code$/, "Got an answer (@{[$rep->code]})"
                );
            }
        }
    }

    # make sure the kid is dead
    wait;
}
