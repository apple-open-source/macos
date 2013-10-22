# -*- perl -*-
#
#   $Id: fork.t,v 1.2 1999/08/12 14:28:59 joe Exp $
#

require 5.004;
use strict;
use IO::Socket ();
use Config ();
use Net::Daemon::Test ();

my $ok;
eval {
  if ($^O ne "MSWin32") {
    my $pid = fork();
    if (defined($pid)) {
      if (!$pid) { exit 0; } # Child
    }
    $ok = 1;
  }
};
if (!$ok) {
  print "1..0\n";
  exit 0;
}


my $numTests = 5;


my($handle, $port);
if (@ARGV) {
    $port = shift @ARGV;
} else {
    ($handle, $port) = Net::Daemon::Test->Child($numTests,
						$^X, '-Iblib/lib',
						'-Iblib/arch',
						't/server', '--mode=fork',
						'--debug', '--timeout', 60);
}

print "Making first connection to port $port...\n";
my $fh = IO::Socket::INET->new('PeerAddr' => '127.0.0.1',
			       'PeerPort' => $port);
printf("%s 1\n", $fh ? "ok" : "not ok");
printf("%s 2\n", $fh->close() ? "ok" : "not ok");
print "Making second connection to port $port...\n";
$fh = IO::Socket::INET->new('PeerAddr' => '127.0.0.1',
			    'PeerPort' => $port);
printf("%s 3\n", $fh ? "ok" : "not ok");
eval {
    for (my $i = 0;  $i < 20;  $i++) {
	print "Writing number: $i\n";
	if (!$fh->print("$i\n")  ||  !$fh->flush()) {
	    die "Client: Error while writing number $i: " . $fh->error()
		. " ($!)";
	}
	print "Written.\n";
	my($line) = $fh->getline();
	if (!defined($line)) {
	    die "Client: Error while reading number $i: " . $fh->error()
		. " ($!)";
	}
	if ($line !~ /(\d+)/  ||  $1 != $i*2) {
	    die "Wrong response, exptected " . ($i*2) . ", got $line";
	}
    }
};
if ($@) {
    print STDERR "$@\n";
    print "not ok 4\n";
} else {
    print "ok 4\n";
}
printf("%s 5\n", $fh->close() ? "ok" : "not ok");

END {
    if ($handle) { $handle->Terminate() }
    if (-f "ndtest.prt") { unlink "ndtest.prt" }
    exit 0;
}
