use strict;
use Test::More;

# here are all the requests the client will try
my @requests = (
    'single.txt', ( 'file1.txt', 'directory/file2.txt', 'ooh.cgi?q=query' ) x 2
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

# create a HTTP::Proxy
my $proxy = HTTP::Proxy->new(
    port     => 0,
    max_keep_alive_requests => 3,    # no more than 3 requests per connection
    max_connections  => 3,    # no more than 3 connections
);
$proxy->init;    # required to access the url later
$proxy->agent->no_proxy( URI->new( $server->url )->host );

# fork the HTTP server
my @pids;
my $pid = fork;
die "Unable to fork web server" if not defined $pid;

if ( $pid == 0 ) {

    # the answer method
    my $answer = sub {
        my $req  = shift;
        my $data = shift;
        my $re   = quotemeta $data;
        like( $req->uri, qr/$re/, "The daemon got what it expected" );
        return HTTP::Response->new( 200, 'OK',
            HTTP::Headers->new( 'Content-Type' => 'text/plain' ),
            "Here is $data." );
    };

    # let's return some files when asked for them
    server_next( $server, $answer, $_ ) for @requests;

    exit 0;
}

# back in the parent
push @pids, $pid;    # remember the kid

# fork a HTTP proxy
fork_proxy(
    $proxy, sub {
        is( $proxy->conn, 3,
            "The proxy served the correct number of connections" );
    }
);

# back in the parent
push @pids, $pid;    # remember the kid

# some variables
my ( $ua, $res, $re );

# the first connection will be closed by the client
$ua = LWP::UserAgent->new;
$ua->proxy( http => $proxy->url );

my $req = shift @requests;
$res =
  $ua->simple_request(
    HTTP::Request->new( GET => $server->url . $req ) );
ok( $res->is_success, "Got an answer (@{[$res->status_line]})" );
$re = quotemeta $req;
like( $res->content, qr/$re/, "The client got what it expected" );

# the other connections (keep-alive)
$ua = LWP::UserAgent->new( keep_alive => 1 );
$ua->proxy( http => $proxy->url );
for (@requests) {
    $res =
      $ua->simple_request( HTTP::Request->new( GET => $server->url . $_ ) );
    ok( $res->is_success, "Got an answer (@{[$res->status_line]})" );
    $re = quotemeta;
    like( $res->content, qr/$re/, "The client got what it expected" );
}

# make sure both kids are dead
wait for @pids;
