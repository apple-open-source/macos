#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/core.t'

use Net::SSLeay;
use Socket;
use IO::Socket::SSL;
eval {require "t/ssl_settings.req";} ||
eval {require "ssl_settings.req";};

$NET_SSLEAY_VERSION = $Net::SSLeay::VERSION;

$numtests = 35;
$|=1;

foreach ($^O) {
    if (/MacOS/ or /VOS/ or /vmesa/ or /riscos/ or /amigaos/) {
	print "1..0 # Skipped: fork not implemented on this platform\n";
	exit;
    }
}

if ($NET_SSLEAY_VERSION < 1.26) {
    print "1..0 \# Skipped: Net::SSLeay version less than 1.26\n";
    exit;
}

print "1..$numtests\n";

my %server_options = (
    SSL_key_file => "certs/server-key.enc", 
    SSL_passwd_cb => sub { return "bluebell" },
    LocalAddr => $SSL_SERVER_ADDR,
    Listen => 2,
    Timeout => 30,
    ReuseAddr => 1,
    SSL_verify_mode => SSL_VERIFY_NONE, 
    SSL_ca_file => "certs/test-ca.pem",
    SSL_cert_file => "certs/server-cert.pem",
    SSL_version => 'TLSv1',
    SSL_cipher_list => 'HIGH'
);


my @servers = (IO::Socket::SSL->new( %server_options),
	       IO::Socket::SSL->new( %server_options),
	       IO::Socket::SSL->new( %server_options));

if (!$servers[0] or !$servers[1] or !$servers[2]) {
    print "not ok # Server init\n";
    exit;
}
&ok("Server initialization");

my ($SSL_SERVER_PORT)  = unpack_sockaddr_in( $servers[0]->sockname );
my ($SSL_SERVER_PORT2) = unpack_sockaddr_in( $servers[1]->sockname );
my ($SSL_SERVER_PORT3) = unpack_sockaddr_in( $servers[2]->sockname );


unless (fork) {
    close $_ foreach @servers;
    my $ctx = IO::Socket::SSL::SSL_Context->new(
	 SSL_passwd_cb => sub { return "opossum" },
    	 SSL_verify_mode => SSL_VERIFY_PEER,
	 SSL_ca_file => "certs/test-ca.pem",
	 SSL_ca_path => '',
	 SSL_version => 'TLSv1',
	 SSL_cipher_list => 'HIGH',
	 SSL_session_cache_size => 4
    );


    if (! defined $ctx->{'session_cache'}) {
	print "not ok \# Context init\n";
	exit;
    }
    &ok("Context init");

    
    # Bogus session test
    unless ($ctx->session_cache("bogus", "bogus", 0)) {
	print "not ";
    }
    &ok("Superficial Cache Addition Test");

    unless ($ctx->session_cache("bogus1", "bogus1", 0)) {
	print "not ";
    }
    &ok("Superficial Cache Addition Test 2");

    my $cache = $ctx->{'session_cache'};

    if (keys(%$cache) != 4) {
	print "not ";
    }
    &ok("Cache Keys Check 1");

    unless ($cache->{'bogus1:bogus1'} and $cache->{'bogus:bogus'}) {
	print "not ";
    }
    &ok("Cache Keys Check 2");

    my ($bogus, $bogus1) = ($cache->{'bogus:bogus'}, $cache->{'bogus1:bogus1'});
    unless ($cache->{'_head'} eq $bogus1) {
	print "not ";
    }
    &ok("Cache Head Check");

    unless ($bogus1->{prev} eq $bogus and
	    $bogus1->{next} eq $bogus and
	    $bogus->{prev} eq $bogus1 and
	    $bogus->{next} eq $bogus1) {
	print "not ";
    }
    &ok("Cache Link Check");


    IO::Socket::SSL::set_default_context($ctx);

    my $sock3 = IO::Socket::INET->new(
    	PeerAddr => $SSL_SERVER_ADDR,
	PeerPort => $SSL_SERVER_PORT3
    );
    my @clients = (
	IO::Socket::SSL->new("$SSL_SERVER_ADDR:$SSL_SERVER_PORT"),
        IO::Socket::SSL->new("$SSL_SERVER_ADDR:$SSL_SERVER_PORT2"),
        IO::Socket::SSL->start_SSL( $sock3 ),
    );
    
    if (!$clients[0] or !$clients[1] or !$clients[2]) {
	print "not ok \# Client init\n";
	exit;
    }
    &ok("Client init");

    # Make sure that first 'bogus' entry has been removed
    if (keys(%$cache) != 6) {
	print "not ";
    }
    &ok("Cache Keys Check 3");

    if ($cache->{'bogus:bogus'}) {
	print "not ";
    }
    &ok("Cache Removal Test");

    if ($cache->{'_head'}->{prev} ne $bogus1) {
	print "not ";
    }
    &ok("Cache Tail Check");

    if ($cache->{'_head'} ne $cache->{"$SSL_SERVER_ADDR:$SSL_SERVER_PORT3"}) {
	print "not ";
    }
    &ok("Cache Insertion Test");

    my @server_ports = ($SSL_SERVER_PORT, $SSL_SERVER_PORT2, $SSL_SERVER_PORT3);
    for (0..2) {
	if (Net::SSLeay::get_session($clients[$_]->_get_ssl_object) ne 
	    $cache->{"$SSL_SERVER_ADDR:$server_ports[$_]"}->{session}) {
	    print "not ";
	}
	&ok("Cache Entry Test $_");
	close $clients[$_];
    }

    @clients = (
    	IO::Socket::SSL->new("$SSL_SERVER_ADDR:$SSL_SERVER_PORT"),
	IO::Socket::SSL->new("$SSL_SERVER_ADDR:$SSL_SERVER_PORT2"),
	IO::Socket::SSL->new("$SSL_SERVER_ADDR:$SSL_SERVER_PORT3")
    );

    if (keys(%$cache) != 6) {
	print "not ";
    }
    &ok("Cache Keys Check 4");

    if (!$cache->{'bogus1:bogus1'}) {
	print "not ";
    }
    &ok("Cache Keys Check 5");

    for (0..2) {
	if (Net::SSLeay::get_session($clients[$_]->_get_ssl_object) ne 
	    $cache->{"$SSL_SERVER_ADDR:$server_ports[$_]"}->{session}) {
	    print "not ";
	}
	&ok("Second Cache Entry Test $_");
	unless ($clients[$_]->print("Test $_\n")) {
	    print "not ";
	}
	&ok("Write Test $_");
	unless ($clients[$_]->readline eq "Ok $_\n") {
	    print "not ";
	}
	&ok("Read Test $_");
	close $clients[$_];
    }

    exit(0);
}

my @clients = map { scalar $_->accept } @servers;
if (!$clients[0] or !$clients[1] or !$clients[2]) {
    print "not ok \# Client init\n";
    exit;
}
&ok("Client init");

close $_ foreach (@clients);


@clients = map { scalar $_->accept } @servers;
if (!$clients[0] or !$clients[1] or !$clients[2]) {
    print $SSL_ERROR;
    print "not ok \# Client init 2\n";
    exit;
}
&ok("Client init 2");

for (0..2) {
    unless ($clients[$_]->readline eq "Test $_\n") {
	print "not ";
    }
    &ok("Server Read $_");
    unless ($clients[$_]->print("Ok $_\n")) {
	print "not ";
    }
    &ok("Server Write $_");
    close $clients[$_];
    close $servers[$_];
}

wait;


sub ok {
    print "ok #$_[0]\n";
}

sub bail {
	print "Bail Out! $IO::Socket::SSL::ERROR";
}
