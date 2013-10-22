#!perl -w

use strict;
use Net::SSLeay;
use Socket;
use IO::Socket::SSL;

if ( grep { $^O =~m{$_} } qw( MacOS VOS vmesa riscos amigaos ) ) {
	print "1..0 # Skipped: fork not implemented on this platform\n";
	exit
}

if ( $^O =~m{mswin32}i ) {
	print "1..0 # Skipped: signals not relevant on this platform\n";
	exit
}

use vars qw( $SSL_SERVER_ADDR );
do "t/ssl_settings.req" || do "ssl_settings.req";

print "1..9\n";

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

	$SIG{HUP} = sub { ok("got hup") };

	close($server);
	my $client = IO::Socket::SSL->new( "$SSL_SERVER_ADDR:$SSL_SERVER_PORT" )
		|| print "not ";
	ok( "client ssl connect" );

	my $line = <$client>;
	print "not " if $line ne "foobar\n";
	ok("got line");

	exit;
}

my $csock = $server->accept;
ok("accept");
$SIG{PIPE} = 'IGNORE';

syswrite($csock,"foo") or print "not ";
ok("wrote foo");
sleep(1);

kill HUP => $pid or print "not ";
ok("send hup");
sleep(1);

syswrite($csock,"bar\n") or print "not ";
ok("wrote bar\\n");

wait;
ok("wait: $?");



sub ok { print "ok #$_[0]\n"; }

