use strict;
use Test::More tests => 28;
use HTTP::Proxy;

# objects
my $proxy  = HTTP::Proxy->new;
my $filter = HTTP::Proxy::HeaderFilter::standard->new;

# a few hacks because we aren't actually connected
$filter->proxy($proxy);

{
    package MockSocket;
    use vars qw( @ISA );
    @ISA = qw( IO::Socket::INET );
    # needed by HTTP::Proxy::HeaderFilter::standard
    sub peerhost { "1.2.3.4"; }
}
$proxy->{client_socket} = MockSocket->new();

# the dummy request
my $req = HTTP::Request->new( GET => 'http://www.example.com/' );
$req->header(
    Proxy_Connection => 'Keep-Alive',
    Connection       => 'Foo, Bar',
    Foo              => 'foofoo',
    Bar              => 'barbar',
    User_Agent       => 'Foo/1.0'
);

$filter->filter( $req->headers, $req );

# hop-by-hop
is( $proxy->hop_headers->header('proxy-connection'),
    'Keep-Alive', "Hop-by-hop Proxy-Connection" );
is( $proxy->hop_headers->header('connection'),
    'Foo, Bar', "Hop-by-hop Connection" );
is( $proxy->hop_headers->header('Foo'), 'foofoo', "Hop-by-hop Foo" );
is( $proxy->hop_headers->header('Bar'), 'barbar', "Hop-by-hop Bar" );

# end-to-end
is( $req->header('user-agent'), 'Foo/1.0', "End-to-end User-Agent" );
is( $req->header('proxy-connection'), undef, "Connection header removed" );
is( $req->header('connection'),       undef, "Connection header removed" );
is( $req->header('Foo'),              undef, "Connection header removed" );
is( $req->header('Bar'),              undef, "Connection header removed" );

# yet another test
$req = HTTP::Request->new( GET => 'http://www.example.com/' );
$req->push_header( Proxy_Connection => 'Keep-Alive' );
$req->push_header( Connection       => 'Foo' );
$req->push_header( Connection       => 'Bar' );
$req->push_header( Foo              => 'foofoo' );
$req->push_header( Bar              => 'barbar' );
$req->push_header( User_Agent       => 'Foo/1.0' );

$filter->filter( $req->headers, $req );

# hop-by-hop
is( $proxy->hop_headers->header('proxy-connection'),
    'Keep-Alive', "Hop-by-hop Proxy-Connection" );
is( $proxy->hop_headers->header('connection'),
    'Foo, Bar', "Hop-by-hop Connection" );
is( $proxy->hop_headers->header('Foo'), 'foofoo', "Hop-by-hop Foo" );
is( $proxy->hop_headers->header('Bar'), 'barbar', "Hop-by-hop Bar" );

# end-to-end
is( $req->header('user-agent'), 'Foo/1.0', "End-to-end User-Agent" );
is( $req->header('proxy-connection'), undef, "Connection header removed" );
is( $req->header('connection'),       undef, "Connection header removed" );
is( $req->header('Foo'),              undef, "Connection header removed" );
is( $req->header('Bar'),              undef, "Connection header removed" );

# a final test
$req = HTTP::Request->new( GET => 'http://www.example.com/' );
$req->push_header( Proxy_Connection => 'Keep-Alive' );
$req->push_header( Connection       => 'Foo, Bar' );
$req->push_header( Connection       => 'Baz' );
$req->push_header( Foo              => 'foofoo' );
$req->push_header( Bar              => 'barbar' );
$req->push_header( Baz              => 'bazbaz' );
$req->push_header( User_Agent       => 'Foo/1.0' );

$filter->filter( $req->headers, $req );

# hop-by-hop
is( $proxy->hop_headers->header('proxy-connection'),
    'Keep-Alive', "Hop-by-hop Proxy-Connection" );
is( $proxy->hop_headers->header('connection'),
    'Foo, Bar, Baz', "Hop-by-hop Connection" );
is( $proxy->hop_headers->header('Foo'), 'foofoo', "Hop-by-hop Foo" );
is( $proxy->hop_headers->header('Bar'), 'barbar', "Hop-by-hop Bar" );
is( $proxy->hop_headers->header('Baz'), 'bazbaz', "Hop-by-hop Baz" );

# end-to-end
is( $req->header('user-agent'), 'Foo/1.0', "End-to-end User-Agent" );
is( $req->header('proxy-connection'), undef, "Connection header removed" );
is( $req->header('connection'),       undef, "Connection header removed" );
is( $req->header('Foo'),              undef, "Connection header removed" );
is( $req->header('Bar'),              undef, "Connection header removed" );

