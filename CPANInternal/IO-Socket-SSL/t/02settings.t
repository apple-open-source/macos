# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/02settings.t'

use IO::Socket::SSL;
eval {require "t/ssl_settings.req";} ||
eval {require "ssl_settings.req";};
use vars qw($SSL_SERVER_ADDR);

print "1..1\n";

$test=1;
my $server = IO::Socket::INET->new( 
    # pick any port on LocalAddr
    LocalAddr => $SSL_SERVER_ADDR,
    Listen => 1
);

if (!$server) {
    print "Bail out! ";
    print("Setup of test IO::Socket::INET server failed: $!.  All the rest of ",
	"the tests in this suite will fail also unless you change the values in ",
	"ssl_settings.req in the t/ directory.");
    exit;
}

print "ok $test\n";
close $server;
