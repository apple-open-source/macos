use strict;
use Test::More;
use LWP::UserAgent;
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter::simple;
use t::Utils;    # some helper functions for the server

if( $^O eq 'MSWin32' ) {
    plan skip_all => "This test fails on MSWin32. HTTP::Proxy is usable on Win32 with maxchild => 0";
    exit;
}

plan tests => 13;

my $test = Test::Builder->new;
my @pids;

# this is to work around tests in forked processes
$test->use_numbers(0);
$test->no_ending(1);

# create a HTTP::Daemon (on an available port)
my $server = server_start();

# create and fork the proxy
my $proxy = HTTP::Proxy->new( port => 0, max_connections => 5 );
$proxy->init;    # required to access the url later
$proxy->agent->no_proxy( URI->new( $server->url )->host );
push @pids, fork_proxy($proxy);

# fork the HTTP server
my $pid = fork;
die "Unable to fork web server" if not defined $pid;

if ( $pid == 0 ) {
    my $res = HTTP::Response->new(
        200, 'OK',
        HTTP::Headers->new( 'Content-Type' => 'text/plain' ),
        "Here is some data."
    );

    # let's return some files when asked for them
    server_next($server) for 1 .. 3;
    server_next($server,
        sub {
            my $req = shift;
            is( $req->header("X-Forwarded-For"), '127.0.0.1',
                "The daemon got X-Forwarded-For" );
            return $res;
        }
    );
    server_next( $server, 
        sub {
            my $req = shift;
            is( $req->header("X-Forwarded-For"), undef,
                "The daemon didn't get X-Forwarded-For" );
            return $res;
        }
    );

    exit 0;
}

push @pids, $pid;

# run a client
my ( $req, $res );
my $ua = LWP::UserAgent->new;
$ua->proxy( http => $proxy->url );

#
# check that we have single Date and Server headers
#

# for GET requests
$req = HTTP::Request->new( GET => $server->url . "headers" );
$res = $ua->simple_request($req);
my @date = $res->headers->header('Date');
is( scalar @date, 1, "A single Date: header for GET request" );
my @server = $res->headers->header('Server');
is( scalar @server, 1, "A single Server: header for GET request" );

# for HEAD requests
$req = HTTP::Request->new( HEAD => $server->url . "headers-head" );
$res = $ua->simple_request($req);
@date = $res->headers->header('Date');
is( scalar @date, 1, "A single Date: header for HEAD request" );
@server = $res->headers->header('Server');
is( scalar @server, 1, "A single Server: header for HEAD request" );

# for direct proxy responses
$ua->proxy( file => $proxy->url );
$req = HTTP::Request->new( GET => "file:///etc/passwd" );
$res = $ua->simple_request($req);
@date = $res->headers->header('Date');
is( scalar @date, 1, "A single Date: header for direct proxy response" );
@server = $res->headers->header('Server');
is( scalar @server, 1, "A single Server: header for direct proxy response" );
# check the Server: header
like( $server[0], qr!HTTP::Proxy/\d+\.\d+!, "Correct server name for direct proxy response" );

# we cannot use a LWP user-agent to check
# that the LWP Client-* headers are removed
use IO::Socket::INET;

# connect directly to the proxy
$proxy->url() =~ /:(\d+)/;
my $sock = IO::Socket::INET->new(
    PeerAddr => 'localhost',
    PeerPort => $1,
    Proto    => 'tcp'
  ) or diag "Can't connect to the proxy";

# send the request
my $url = $server->url;
$url =~ m!http://([^:]*)!;
print $sock "GET $url HTTP/1.0\015\012Host: $1\015\012\015\012";  

# fetch and count the Client-* response headers
my @client = grep { /^Client-/ } <$sock>;
is( scalar @client, 0, "No Client-* headers sent by the proxy" );

# close the connection to the proxy
close $sock or diag "close: $!";

# X-Forwarded-For (test in the server)
$req = HTTP::Request->new( HEAD => $server->url . "x-forwarded-for" );
$res = $ua->simple_request($req);
is( $res->header( 'X-Forwarded-For' ), undef, "No X-Forwarded-For sent back" );

# yet another proxy
$proxy = HTTP::Proxy->new( port => 0, max_connections => 1, x_forwarded_for => 0 );
$proxy->init;    # required to access the url later
$proxy->agent->no_proxy( URI->new( $server->url )->host );
$proxy->push_filter( response => HTTP::Proxy::HeaderFilter::simple->new(
    sub { is( $_[0]->proxy->client_headers->header("Client-Response-Num"), 1,
          "Client headers" ); } ) );
push @pids, fork_proxy($proxy);

# X-Forwarded-For (test in the server)
$ua->proxy( http => $proxy->url );
$req = HTTP::Request->new( HEAD => $server->url . "x-forwarded-for" );
$res = $ua->simple_request($req);
is( $res->header( 'X-Forwarded-For' ), undef, "No X-Forwarded-For sent back" );

# make sure both kids are dead
wait for @pids;

