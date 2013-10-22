#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/nonblock.t'

# Tests the use if SSL_cert instead of SSL_cert_file
# because Net::SSLeay does not implement the necessary functions
# to create a X509 from file/string (PEM_read_bio_X509) I just
# create a server with SSL_cert_file and get the X509 from it using
# Net::SSLeay::get_certificate.
# Test should also test if SSL_cert is an array of X509* 
# and if SSL_key is an EVP_PKEY* but with the current function in
# Net::SSLeay I don't see a way to test it

use Net::SSLeay;
use Socket;
use IO::Socket::SSL;
use strict;

use vars qw( $SSL_SERVER_ADDR );
do "t/ssl_settings.req" || do "ssl_settings.req";

if ( grep { $^O =~m{$_} } qw( MacOS VOS vmesa riscos amigaos ) ) {
    print "1..0 # Skipped: fork not implemented on this platform\n";
    exit
}

$|=1;
print "1..9\n";

my $ID = 'server';
my %server_args = (
    LocalAddr => $SSL_SERVER_ADDR,
    Listen => 2,
    ReuseAddr => 1,
    SSL_server => 1,
    SSL_verify_mode => 0x00,
    SSL_ca_file => "certs/test-ca.pem",
    SSL_key_file => "certs/client-key.pem",
);

my ($x509,@server);
foreach my $test ( 1,2,3 ) {
    my %args = %server_args;
    my $spec;
    if ( $test == 1 ) {
	# 1st test:  create server with SSL_cert_file
	$args{SSL_cert_file} = "certs/client-cert.pem";
	$spec = 'Using SSL_cert_file';
    } elsif ( $test == 2 ) {
    	# 2nd test:  use x509 from previous server
	# with SSL_cert instead of SSL_cert_file
	$args{SSL_cert} = $x509;
	$spec = 'Using SSL_cert';
    } elsif ( $test == 3 ) {
    	# 3rd test: empty SSL_cert, so that default
	# SSL_cert_file gets not used
	# server creation should fail
	$spec = 'Empty SSL_cert';
	$args{SSL_cert} = undef;
    }

    # create server
    my $server = IO::Socket::SSL->new( %args ) || do {
	notok( "$spec: $!" );
	next;
    };
    my ($port) = unpack_sockaddr_in( getsockname($server) );
    #DEBUG( "Server listening to $SSL_SERVER_ADDR:$port" );
    ok("Server Initialization $spec");
    push @server,$server;


    # then connect to it from a child
    defined( my $pid = fork() ) || die $!;
    if ( $pid == 0 ) {
	close($server);
	$ID = 'client';

	my $to_server = IO::Socket::SSL->new( 
	    PeerAddr => $SSL_SERVER_ADDR,
	    PeerPort => $port,
	    SSL_verify_mode => 0x00,
	);
	if ( $test == 3 ) {
    	    notok( "$spec: connect suceeded" ) if $to_server;
	    ok( "$spec: connect failed" );
	    exit;
	} elsif ( ! $to_server ) {
	    notok( "connect failed: $!" );
	    exit
	};
	ok( "client connected $spec" );
	<$to_server>; # wait for close from parent
	exit;
    } 

    my $to_client = $server->accept;
    if ( $test == 3 ) {
    	notok( "$spec: accept suceeded" ) if $to_client;
	ok( "$spec: accept failed" );
    } elsif ( ! $to_client ) {
	notok( "$spec: accept failed: $!" );
	kill(9,$pid);
    } else {
    	ok( "Server accepted $spec" );
	# save the X509 certificate from the server
	$x509 ||= Net::SSLeay::get_certificate($to_client->_get_ssl_object);
    }

    close($to_client) if $to_client;
    wait;
}



sub ok { print "ok # [$ID] @_\n"; }
sub notok { print "not ok # [$ID] @_\n"; }
