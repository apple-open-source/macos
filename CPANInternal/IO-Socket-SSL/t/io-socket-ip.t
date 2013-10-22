#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/dhe.t'

use Net::SSLeay;
use Socket;
use IO::Socket::SSL;
use strict;


if ( grep { $^O =~m{$_} } qw( MacOS VOS vmesa riscos amigaos ) ) {
    print "1..0 # Skipped: fork not implemented on this platform\n";
    exit
}

# check if we have loaded IO::Socket::IP, IO::Socket::SSL should do it by
# itself if it is available
unless( IO::Socket::SSL->CAN_IPV6 eq "IO::Socket::IP" ) {
	# not available or IO::Socket::SSL forgot to load it
	if ( ! eval { require IO::Socket::IP; IO::Socket::IP->VERSION(0.11) } ) {
		print "1..0 # Skipped: no IO::Socket::IP 0.11 available\n";
	} else {
		print "1..1\nnot ok # automatic use of IO::Socket::IP\n";
	}
	exit
}

my $addr = '::1';
# check if we can use ::1, e.g if the computer has IPv6 enabled
if ( ! IO::Socket::IP->new(
	Listen => 10,
	LocalAddr => $addr,
)) {
	print "1..0 # no IPv6 enabled on this computer\n";
	exit
}

$|=1;
print "1..3\n";

# first create simple ssl-server
my $ID = 'server';
my $server = IO::Socket::SSL->new(
    LocalAddr => $addr,
    Listen => 2,
    SSL_cert_file => "certs/server-cert.pem",
    SSL_key_file  => "certs/server-key.pem",
) || do {
    notok($!);
    exit
};
ok("Server Initialization at $addr");

# add server port to addr
$addr = "[$addr]:".$server->sockport;
print "# server at $addr\n";

my $pid = fork();
if ( !defined $pid ) {
    die $!; # fork failed

} elsif ( !$pid ) {    ###### Client

    $ID = 'client';
    close($server);
    my $to_server = IO::Socket::SSL->new( $addr ) || do {
    	notok( "connect failed: ".IO::Socket::SSL->errstr() );
		exit
    };
    ok( "client connected" );

} else {                ###### Server

    my $to_client = $server->accept || do {
    	notok( "accept failed: ".$server->errstr() );
		kill(9,$pid);
		exit;
    };
    ok( "Server accepted" );
    wait;
}

sub ok { print "ok # [$ID] @_\n"; }
sub notok { print "not ok # [$ID] @_\n"; }
