#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/nonblock.t'


use Net::SSLeay;
use Socket;
use IO::Socket::SSL;
use IO::Select;
use Errno qw(EAGAIN EINPROGRESS );
use strict;

use vars qw( $SSL_SERVER_ADDR );
do "t/ssl_settings.req" || do "ssl_settings.req";

if ( grep { $^O =~m{$_} } qw( MacOS VOS vmesa riscos amigaos ) ) {
    print "1..0 # Skipped: fork not implemented on this platform\n";
    exit
}
    
$|=1;
print "1..9\n";


my $server = IO::Socket::INET->new(
    LocalAddr => $SSL_SERVER_ADDR,
    Listen => 2,
    ReuseAddr => 1,
);
print("not ok\n"),exit if !$server;
ok("Server Initialization");
my ($SSL_SERVER_PORT) = unpack_sockaddr_in( $server->sockname );


defined( my $pid = fork() ) || die $!;
if ( $pid == 0 ) {
    client();
} else {
    server();
    #kill(9,$pid);
    wait;
}


sub client {
    close($server);
    my $client = IO::Socket::INET->new( "$SSL_SERVER_ADDR:$SSL_SERVER_PORT" )
	or return fail("client tcp connect");
    ok("client tcp connect");

    IO::Socket::SSL->start_SSL($client) and
	return fail('start ssl should fail');
    ok("startssl client failed: $SSL_ERROR");

    UNIVERSAL::isa($client,'IO::Socket::INET') or
    	return fail('downgrade socket after error');
    ok('downgrade socket after error');

    print $client "foo\n" or  return fail("send to server: $!");
    ok("send to server");
    my $l;
    while (defined($l = <$client>)) {
    	if ( $l =~m{bar\n} ) {
	    return ok('client receive non-ssl data');
	}
	#warn "XXXXXXXX $l";
    }
    fail("receive non-ssl data");
}

sub server {
    my $csock = $server->accept or return fail('tcp accept');
    ok('tcp accept');
    print $csock "This is no SSL handshake\n";
    ok('send non-ssl data');

    alarm(10);
    my $l;
    while (defined( $l = <$csock>)) {
	if ($l =~m{foo\n} ) {
	    print $csock "bar\n";
	    return ok("received non-ssl data");
	}
	#warn "XXXXXXXXX $l";
    }
    fail('no data from client'.$!);
}


sub ok { print "ok #$_[0]\n"; return 1 }
sub fail { print "not ok #$_[0]\n"; return }

