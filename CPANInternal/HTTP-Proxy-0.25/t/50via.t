use strict;
use Test::More;
use LWP::UserAgent;
use HTTP::Proxy;
use t::Utils;    # some helper functions for the server

if( $^O eq 'MSWin32' ) {
    plan skip_all => "This test fails on MSWin32. HTTP::Proxy is usable on Win32 with maxchild => 0";
    exit;
}

plan tests => 4;

my $test = Test::Builder->new;
my @pids;

# this is a rather long test suite just to test that
# $proxy->via() works ok

# this is to work around tests in forked processes
$test->use_numbers(0);
$test->no_ending(1);

# create a HTTP::Daemon (on an available port)
my $server = server_start();

# create and fork the proxy
# the proxy itself will not fork
my $proxy = HTTP::Proxy->new( port => 0, max_connections => 1, max_clients => 0 );
$proxy->init;    # required to access the url later
$proxy->agent->no_proxy( URI->new( $server->url )->host );
push @pids, fork_proxy($proxy);

# fork the HTTP server
my $pid = fork;
die "Unable to fork web server" if not defined $pid;

if ( $pid == 0 ) {

    # the answer method
    my $answer1 = sub {
        my ( $req, $data ) = @_;
        isnt( $req->headers->header('Via'), undef, "Server says Via: header added" );
        return HTTP::Response->new(
            200, 'OK',
            HTTP::Headers->new( 'Content-Type' => 'text/plain' ),
            "Headers checked."
        );
    };
    my $answer2 = sub {
        my ( $req, $data ) = @_;
        is( $req->headers->header('Via'), undef, "Server says no Via: header added" );
        return HTTP::Response->new(
            200, 'OK',
            HTTP::Headers->new( 'Content-Type' => 'text/plain' ),
            "Headers checked."
        );
    };

    # let's return some files when asked for them
    server_next( $server, $answer1 );
    server_next( $server, $answer2 );

    exit 0;
}

push @pids, $pid;

# run a client
my ( $req, $res );
my $ua = LWP::UserAgent->new;
$ua->proxy( http => $proxy->url );

# send a Proxy-Connection header
$req = HTTP::Request->new( GET => $server->url );
$res = $ua->simple_request($req);
isnt( $res->headers->header('Via'), undef, "Client says Via: header added" );

# create and fork the proxy
$proxy->via('');
push @pids, fork_proxy($proxy);
$res = $ua->simple_request($req);
is( $res->headers->header('Via'), undef, "Client says no Via: header added" );


# make sure both kids are dead
wait for @pids;

