# -*- perl -*-
#

require 5.004;
use strict;

require "t/lib.pl";


my $numTests = 10;
my $numTest = 0;




my($handle, $port);
if (@ARGV) {
    $port = $ARGV[0];
} else {
    ($handle, $port) = Net::Daemon::Test->Child($numTests,
						$^X, '-Iblib/lib',
						'-Iblib/arch',
						't/server', '--mode=single',
						'--debug', '--timeout', 60);
}

my @opts = ('peeraddr' => '127.0.0.1', 'peerport' => $port, 'debug' => 1,
	    'application' => 'CalcServer', 'version' => 0.01,
	    'timeout' => 20);
my $client;

# Making a first connection and closing it immediately
Test(eval { RPC::PlClient->new(@opts) })
    or print "Failed to make first connection: $@\n";

RunTests(@opts);
eval { $handle->Terminate() } if $handle;
