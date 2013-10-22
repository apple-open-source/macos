# -*- perl -*-
#
#   $Id: single.t,v 1.2 1999/08/12 14:28:59 joe Exp $
#

require 5.004;
use strict;

use IO::Socket ();
use Config ();
use Net::Daemon::Test ();

my $numTests = 5;


my($handle, $port);
if (@ARGV) {
    $port = shift @ARGV;
} else {
    ($handle, $port) =
	Net::Daemon::Test->Child($numTests,
					      $^X, '-Iblib/lib', '-Iblib/arch',
					      't/server', '--mode=single',
					      '--timeout', 60);
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
my($ok) = $fh ? 1 : 0;
for (my $i = 0;  $ok  &&  $i < 20;  $i++) {
    print "Writing number: $i\n";
    if (!$fh->print("$i\n")  ||  !$fh->flush()) { $ok = 0; last; }
    print "Written.\n";
    my($line) = $fh->getline();
    print "line = ", (defined($line) ? $line : "undef"), "\n";
    if (!defined($line)) { $ok = 0;  last; }
    if ($line !~ /(\d+)/  ||  $1 != $i*2) { $ok = 0;  last; }
}
printf("%s 4\n", $ok ? "ok" : "not ok");
printf("%s 5\n", $fh->close() ? "ok" : "not ok");

END {
    if ($handle) { $handle->Terminate() }
    if (-f "ndtest.prt") { unlink "ndtest.prt" }
}
