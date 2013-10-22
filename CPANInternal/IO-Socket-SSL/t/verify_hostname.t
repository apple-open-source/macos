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

# if we have an IDN library max the IDN tests too
my $can_idn  = eval { require Encode } &&
	( eval { require Net::LibIDN } || eval { require Net::IDN::Encode } || eval { require URI; URI->VERSION(1.50) } );

$|=1;
my $max = 40;
$max+=3 if $can_idn;
print "1..$max\n";

my $server = IO::Socket::SSL->new(
	LocalAddr => $SSL_SERVER_ADDR,
	Listen => 2,
	ReuseAddr => 1,
	SSL_server => 1,
	#SSL_verify_mode => 0x00,
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

	close($server);
	my $client = IO::Socket::SSL->new( "$SSL_SERVER_ADDR:$SSL_SERVER_PORT" )
		|| print "not ";
	ok( "client ssl connect" );

	my $issuer = $client->peer_certificate( 'issuer' );
	print "not " if $issuer !~m{IO::Socket::SSL Demo CA};
	ok("issuer");

	my $cn = $client->peer_certificate( 'cn' );
	print "not " unless $cn eq "server.local";
	ok("cn");

	my @alt = $client->peer_certificate( 'subjectAltNames' );
	my @want = ( 
		GEN_DNS() => '*.server.local',
		GEN_IPADD() => '127.0.0.1',
		GEN_DNS() => 'www*.other.local',
		GEN_DNS() => 'smtp.mydomain.local',
		GEN_DNS() => 'xn--lwe-sna.idntest.local',
	);
	while (@want) {
		my ($typ,$text) = splice(@want,0,2);
		my $data = ($typ == GEN_IPADD() ) ? inet_aton($text):$text;
		my ($th,$dh) = splice(@alt,0,2);
		$th == $typ and $dh eq $data or print "not ";
		ok( $text );
	}
	@alt and print "not ";
	ok( 'no more altSubjectNames' );

	my @tests = (
		'127.0.0.1' => [qw( smtp ldap www)],
		'server.local' => [qw(smtp ldap)],
		'blafasel.server.local' => [qw(ldap www)],
		'lala.blafasel.server.local' => [],
		'www.other.local' => [qw(www)],
		'www-13.other.local' => [qw(www)],
		'www-13.lala.other.local' => [],
		'smtp.mydomain.local' => [qw(smtp ldap www)],
		'xn--lwe-sna.idntest.local' => [qw(smtp ldap www)],
		'smtp.mydomain.localizing.useless.local' => [],
	);
	if ( $can_idn ) {
		# check IDN handling
		my $loewe = "l\366we.idntest.local";
		push @tests, ( $loewe => [qw(smtp ldap www)] );
	}

	while (@tests) {
		my ($host,$expect) = splice(@tests,0,2);
		my %expect = map { $_=>1 } @$expect;
		for my $typ (qw( smtp ldap www)) {
			my $is = $client->verify_hostname( $host, $typ ) ? 'pass':'fail';
			my $want = $expect{$typ} ? 'pass':'fail';
			print "not " if $is ne $want;
			ok( "$want $host $typ" );
		}
	}

	exit;
}

my $csock = $server->accept;
wait;



sub ok { print "ok #$_[0]\n"; }

