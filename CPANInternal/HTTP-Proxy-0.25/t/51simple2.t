use Test::More tests => 2;
use strict;
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter::simple;
use t::Utils;

# create the filter
my $sub = sub {
    my ( $self, $headers, $message) = @_;
    $headers->header( X_Foo => 'Bar' );
};

my $filter = HTTP::Proxy::HeaderFilter::simple->new($sub);

# create the proxy
my $proxy = HTTP::Proxy->new(
    port     => 0,
    max_clients => 0,
    max_connections  => 2,
);

# prepare the proxy and server
$proxy->init;
$proxy->agent->proxy( http => "" );
$proxy->push_filter( response => $filter );
my $url = $proxy->url;

my $server = server_start();
my $serverurl = $server->url;

# fork the proxy
my @pids;
push @pids, fork_proxy($proxy);

# fork the HTTP server
my $pid = fork;
die "Unable to fork web server" if not defined $pid;

if ( $pid == 0 ) {
    server_next($server) for 1 .. 2;
    exit 0;
}
push @pids, $pid;

#
# check that the correct transformation is applied
#

# for GET requests
my $ua = LWP::UserAgent->new();
$ua->proxy( http => $url );
my $response = $ua->request( HTTP::Request->new( GET => $serverurl ) );
is( $response->header( "X-Foo" ), "Bar", "Proxy applied the transformation" );

# for HEAD requests
$ua = LWP::UserAgent->new();
$ua->proxy( http => $url );
$response = $ua->request( HTTP::Request->new( HEAD => $serverurl ) );
is( $response->header( "X-Foo" ), "Bar", "Proxy applied the transformation" );

# wait for kids
wait for @pids;

