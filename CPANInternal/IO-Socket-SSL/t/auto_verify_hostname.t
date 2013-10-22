#!perl -w

use strict;
use Net::SSLeay;
use Socket;
use IO::Socket::SSL;

if ( grep { $^O =~m{$_} } qw( MacOS VOS vmesa riscos amigaos ) ) {
	print "1..0 # Skipped: fork not implemented on this platform\n";
	exit
}

# subjectAltNames are not supported or buggy in older versions,
# so certificates cannot be checked
if ( $Net::SSLeay::VERSION < 1.33 ) {
	print "1..0 # Skipped because of \$Net::SSLeay::VERSION= $Net::SSLeay::VERSION <1.33\n";
	exit;
}

use vars qw( $SSL_SERVER_ADDR );
do "t/ssl_settings.req" || do "ssl_settings.req";

$|=1;
print "1..30\n";

my $server = IO::Socket::SSL->new(
	LocalAddr => $SSL_SERVER_ADDR,
	Listen => 2,
	ReuseAddr => 1,
	SSL_server => 1,
	SSL_ca_file => "certs/test-ca.pem",
	SSL_cert_file => "certs/server-wildcard.pem",
	SSL_key_file => "certs/server-wildcard.pem",
);
warn "\$!=$!, \$\@=$@, S\$SSL_ERROR=$SSL_ERROR" if ! $server;
print "not ok\n", exit if !$server;
ok("Server Initialization");
my $SSL_SERVER_PORT = $server->sockport;

defined( my $pid = fork() ) || die $!;
if ( $pid == 0 ) {
	while (1) {
		my $csock = $server->accept || next;
		print $csock "hallo\n";
	}
}

close($server);
my @tests = qw(
	example.com      www FAIL
	server.local     ldap OK
	server.local     www FAIL
	bla.server.local www OK
	www7.other.local www OK
	www7.other.local ldap FAIL
	bla.server.local ldap OK
);

for( my $i=0;$i<@tests;$i+=3 ) {
	my ($name,$scheme,$result) = @tests[$i,$i+1,$i+2];
	my $cl = IO::Socket::SSL->new(
		SSL_ca_file => 'certs/test-ca.pem',
		PeerAddr => "$SSL_SERVER_ADDR:$SSL_SERVER_PORT",
		SSL_verify_mode => 1,
		SSL_verifycn_scheme => $scheme,
		SSL_verifycn_name => $name,
	);
	if ( $result eq 'FAIL' ) {
		print "not " if $cl;
		ok( "connection to $name/$scheme failed" );
	} else {
		print "not " if !$cl;
		ok( "connection to $name/$scheme succeeded" );
	}
	$cl || next;
	print "not " if <$cl> ne "hallo\n";
	ok( "received hallo" );
}

for( my $i=0;$i<@tests;$i+=3 ) {
	my ($name,$scheme,$result) = @tests[$i,$i+1,$i+2];
	my $cl = IO::Socket::INET->new(
		PeerAddr => "$SSL_SERVER_ADDR:$SSL_SERVER_PORT",
	) || print "not ";
	ok( "tcp connect" );
	$cl = IO::Socket::SSL->start_SSL( $cl,
		SSL_ca_file => 'certs/test-ca.pem',
		SSL_verify_mode => 1,
		SSL_verifycn_scheme => $scheme,
		SSL_verifycn_name => $name,
	);
	if ( $result eq 'FAIL' ) {
		print "not " if $cl;
		ok( "ssl upgrade of connection to $name/$scheme failed" );
	} else {
		print "not " if !$cl;
		ok( "ssl upgrade of connection to $name/$scheme succeeded" );
	}
	$cl || next;
	print "not " if <$cl> ne "hallo\n";
	ok( "received hallo" );
}

kill(9,$pid);
wait;

sub ok { print "ok #$_[0]\n"; }

