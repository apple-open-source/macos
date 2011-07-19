use strict;
use Test::More;
use t::Utils;
use LWP::UserAgent;
use HTTP::Proxy;

# here are all the requests the client will try
my @requests = (

    # host, path, expected code, dns should fail
    [ 'www.mongueurs.net', '/',         200 ],
    [ 'httpd.apache.org',  '/docs',     301 ],
    [ 'www.google.com',    '/testing/', 404 ],
    [ 'www.error.zzz', '/', '5..', 1 ],
);

if ( $^O eq 'MSWin32' ) {
    plan skip_all =>
        "This test fails on MSWin32. HTTP::Proxy is usable on Win32 with maxchild => 0";
    exit;
}

# we skip the tests if the network is not available
my $web_ok = web_ok();
plan tests => @requests + 2;

# this is to work around tests in forked processes
my $test = Test::Builder->new;
$test->use_numbers(0);
$test->no_ending(1);

my $proxy = HTTP::Proxy->new(
    port            => 0,
    max_connections => @requests * $web_ok + 1,
);
$proxy->init;    # required to access the url later

# fork a HTTP proxy
my $pid = fork_proxy(
    $proxy,
    sub {
        is( $proxy->conn,
            @requests * $web_ok + 1,
            "Served the correct number of requests"
        );
    }
);

# no Host: header
my $content = bare_request( '/', HTTP::Headers->new(), $proxy );
my ($code) = $content =~ m!^HTTP/\d+\.\d+ (\d\d\d) !g;
is( $code, 400, "Relative URL and no Host: Bad Request" );

SKIP: {
    skip "Web does not seem to work", scalar @requests unless $web_ok;

    for (@requests) {
        my ( $host, $path, $status, $dns_fail ) = @$_;
        $dns_fail &&= defined +( gethostbyname $host )[4];

    SKIP: {
            if ($dns_fail) {
                $content = bare_request( '/',
                    HTTP::Headers->new( Host => 'localhost' ), $proxy );
                skip "Our DNS shouldn't resolve $host", 1;
            }
            else {
                $content = bare_request( $path,
                    HTTP::Headers->new( Host => $host ), $proxy );
                ($code) = $content =~ m!^HTTP/\d+\.\d+ (\d\d\d) !g;
                like( $code, qr/^$status$/, "Got an answer ($code)" );
            }
        }
    }
}

# make sure the kid is dead
wait;

