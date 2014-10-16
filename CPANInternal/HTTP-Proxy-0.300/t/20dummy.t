use Test::More;
use strict;

# here are all the requests the client will try
my @requests = qw(
    file1.txt
    directory/file2.txt
    ooh.cgi?q=query
);

if( $^O eq 'MSWin32' ) {
    plan skip_all => "This test fails on MSWin32. HTTP::Proxy is usable on Win32 with maxchild => 0";
    exit;
}

plan tests => 3 * @requests + 1;

use LWP::UserAgent;
use HTTP::Proxy;
use t::Utils;    # some helper functions for the server

my $test = Test::Builder->new;

# this is to work around tests in forked processes
$test->use_numbers(0);
$test->no_ending(1);

# create a HTTP::Daemon (on an available port)
my $server = server_start();
my $serverurl = $server->url;

my $proxy = HTTP::Proxy->new( port => 0, max_connections => scalar @requests );
$proxy->init;    # required to access the url later
$proxy->agent->no_proxy( URI->new( $server->url )->host );
my $proxyurl = $proxy->url;

# fork the HTTP server
my @pids;
my $pid = fork;
die "Unable to fork web server" if not defined $pid;

if ( $pid == 0 ) {

    # the answer method
    my $answer = sub {
        my $req  = shift;
        my $data = shift;
        my $re = quotemeta $data;
        like( $req->uri, qr/$re/, "The daemon got what it expected" );
        return HTTP::Response->new(
            200, 'OK',
            HTTP::Headers->new( 'Content-Type' => 'text/plain' ),
            "Here is $data."
        );
    };

    # let's return some files when asked for them
    server_next( $server, $answer, $_ ) for @requests;

    exit 0;
}

# back in the parent
push @pids, $pid;    # remember the kid

# fork a HTTP proxy
$pid = fork_proxy(
    $proxy,
    sub {
        is( $proxy->conn, scalar @requests,
            "The proxy served the correct number of connections" );
    }
);

# back in the parent
push @pids, $pid;    # remember the kid

# run a client
my $ua = LWP::UserAgent->new;
$ua->proxy( http => $proxyurl );

for (@requests) {
    my $req = HTTP::Request->new( GET => $serverurl . $_ );
    my $rep = $ua->simple_request($req);
    ok( $rep->is_success, "Got an answer (@{[$rep->status_line]})" );
    my $re = quotemeta;
    like( $rep->content, qr/$re/, "The client got what it expected" );
}

# make sure both kids are dead
wait for @pids;
