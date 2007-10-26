use Test::More tests => 4;
use strict;
use t::Utils; use HTTP::Proxy;
use LWP::UserAgent;
use IO::Socket::INET;

# test CONNECT
my $test = Test::Builder->new;

# this is to work around tests in forked processes
$test->use_numbers(0);
$test->no_ending(1);

my $host = 'www.perl.org:22';

SKIP:
{
    # check if we can actually connect
    my $sock = IO::Socket::INET->new( PeerAddr => $host )
      or skip "Direct connection to $host impossible", 4;
    my $banner = <$sock>;
    close $sock;
    
    my $proxy = HTTP::Proxy->new( port => 0, max_connections => 1 );
    $proxy->init;    # required to access the url later

    # fork a HTTP proxy
    my $pid = fork_proxy(
        $proxy,
        sub {
            ok( $proxy->conn == 1, "Served the correct number of requests" );
        }
    );

    # run a client
    my $ua = LWP::UserAgent->new;
    $ua->proxy( https => $proxy->url );

    my $req = HTTP::Request->new( CONNECT => "https://$host/" );
    my $res = $ua->request($req);
    $sock = $res->{client_socket};

    my $read;
    is( $res->code, 200, "The proxy accepts CONNECT requests" );
    ok( $sock->sysread( $read, 100 ), "Read some data from the socket" );
    is( $read, $banner, "CONNECTed to the TCP server" );
    close $sock;

    # make sure the kid is dead
    wait;
}
