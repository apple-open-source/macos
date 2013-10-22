#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/dhe.t'

# This tests the use of Diffie Hellman Key Exchange (DHE)
# If you have only a 384bit RSA key you can not use RSA key exchange,
# but DHE is usable. For an explanation see
# http://groups.google.de/group/mailing.openssl.users/msg/d60330cfa7a6034b
# So this test simple uses a 384bit RSA key to make sure that DHE is used.

use Net::SSLeay;
use Socket;
use IO::Socket::SSL;
use strict;


if ( grep { $^O =~m{$_} } qw( MacOS VOS vmesa riscos amigaos ) ) {
    print "1..0 # Skipped: fork not implemented on this platform\n";
    exit
}

$|=1;
print "1..3\n";

# first create simple ssl-server
my $ID = 'server';
my $addr = '127.0.0.1';
my $server = IO::Socket::SSL->new(
    LocalAddr => $addr,
    Listen => 2,
    ReuseAddr => 1,
    SSL_cert_file => "certs/server-rsa384-dh.pem",
    SSL_key_file  => "certs/server-rsa384-dh.pem",
    SSL_dh_file   => "certs/server-rsa384-dh.pem",
    # at least 0.9.8[ab] have problems if we don't explicitly disable
    # RSA or EXPORT56, and 1.0.1 complains if we have RSA authentication
    # enabled 
    SSL_cipher_list => 'ALL:RSA:!aRSA',
) || do {
    notok($!);
    exit
};
ok("Server Initialization");

# add server port to addr
$addr.= ':'.(sockaddr_in( getsockname( $server )))[0];

my $pid = fork();
if ( !defined $pid ) {
    die $!; # fork failed

} elsif ( !$pid ) {    ###### Client

    $ID = 'client';
    close($server);
    my $to_server = IO::Socket::SSL->new( $addr ) || do {
    	notok( "connect failed: $SSL_ERROR" );
	exit
    };
    ok( "client connected" );

} else {                ###### Server

    my $to_client = $server->accept || do {
    	notok( "accept failed: $SSL_ERROR" );
	kill(9,$pid);
	exit;
    };
    ok( "Server accepted" );
    wait;
}

sub ok { print "ok # [$ID] @_\n"; }
sub notok { print "not ok # [$ID] @_\n"; }
