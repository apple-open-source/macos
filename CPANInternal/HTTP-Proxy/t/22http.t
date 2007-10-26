use strict;
use vars qw( @requests );

# here are all the requests the client will try
BEGIN {
    @requests = (
        [ 'http://www.mongueurs.net/',    200 ],
        [ 'http://httpd.apache.org/docs', 301 ],
        [ 'http://www.google.com/testing/', 404 ],
        [ 'http://www.error.zzz/',        500 ],
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

    my $proxy = HTTP::Proxy->new( port => 0, max_connections => scalar @requests );
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
        my $req = HTTP::Request->new( GET => $_->[0] );
        my $rep = $ua->simple_request($req);
        is( $rep->code, $_->[1], "Got an answer (@{[$rep->code]})" );
    }

    # make sure the kid is dead
    wait;
}
