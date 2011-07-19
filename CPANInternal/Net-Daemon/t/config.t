# -*- perl -*-
#
#   $Id: config.t,v 1.2 1999/08/12 14:28:59 joe Exp $
#
require 5.004;
use strict;

use IO::Socket ();
use Config ();
use Net::Daemon::Test ();
use Socket ();


my $CONFIG_FILE = "t/config";
my $numTests = 5;


sub RunTest ($$) {
    my $config = shift;  my $numTests = shift;

    if (!open(CF, ">$CONFIG_FILE")  ||  !(print CF $config)  ||  !close(CF)) {
	die "Error while creating config file $CONFIG_FILE: $!";
    }

    my($handle, $port) = Net::Daemon::Test->Child
	($numTests, $^X, '-Iblib/lib', '-Iblib/arch', 't/server', '--debug',
	 '--mode=single', '--configfile', $CONFIG_FILE);
    my $fh = IO::Socket::INET->new('PeerAddr' => '127.0.0.1',
				   'PeerPort' => $port);
    my $result;
    my $success = $fh && $fh->print("1\n")  &&
	defined($result = $fh->getline())  &&  $result =~ /2/;
    $handle->Terminate();
    $success ? "" : "not ";
}


print "Testing config file with open client list.\n";
my $ok = RunTest(q/{'mode' => 'single', 'timeout' => 60}/,
		$numTests);
print "${ok}ok 1\n";

print "Testing config file with client 127.0.0.1.\n";
$ok = RunTest(q/
    { 'mode' => 'single',
      'timeout' => 60,
      'clients' => [ { 'mask' => '^127\.0\.0\.1$', 'accept' => 1 },
                     { 'mask' => '.*', 'accept' => 0 }
                   ]
    }/, undef);
print "${ok}ok 2\n";

print "Testing config file with client !127.0.0.1.\n";
$ok = RunTest(q/
    { 'mode' => 'single',
      'timeout' => 60,
      'clients' => [ { 'mask' => '^127\.0\.0\.1$', 'accept' => 0 },
                     { 'mask' => '.*', 'accept' => 1 }
                   ]
    }/, undef);
print(($ok ? "" : "not "), "ok 3\n");

my $hostname = gethostbyaddr(Socket::inet_aton("127.0.0.1"),
			   Socket::AF_INET());
if ($hostname) {
    my $regexp = $hostname;
    $regexp =~ s/\./\\\./g;
    print "Testing config file with client $hostname.\n";
    $ok = RunTest(q/
    { 'mode' => 'single',
      'timeout' => 60,
      'clients' => [ { 'mask' => '^/
 . $regexp . q/$', 'accept' => 1 },
                     { 'mask' => '.*', 'accept' => 0 }
                   ]
    }/, undef);
    print "${ok}ok 4\n";

    print "Testing config file with client !$hostname\n";
    $ok = RunTest(q/
    { 'mode' => 'single',
      'timeout' => 60,
      'clients' => [ { 'mask' => '^/
 . $regexp . q/$', 'accept' => 0 },
                     { 'mask' => '.*', 'accept' => 1 }
                   ]
    }/, undef);
    print(($ok ? "" : "not "), "ok 5\n");    
} else {
    print "ok 4 # skip\n";
    print "ok 5 # skip\n";
}

END {
    if (-f "ndtest.prt") { unlink "ndtest.prt" }
}
