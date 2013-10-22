# -*- perl -*-
#
#   $Id: thread.t,v 1.2 1999/08/12 14:28:59 joe Exp $
#

require 5.004;
use strict;

BEGIN { push(@INC, "C:/temp/Net-Daemon/blib/lib");}
use IO::Socket ();
use Config ();
use Net::Daemon::Test ();
use Test::More;

my $numTests = 5;

# Check whether threads are available, otherwise skip this test.
my $version = $^V;
$version =~ s/v(\d+\.\d+)\.\d+/$1/;


if ($version >= 5.10) {

    # The paragraph below is pasted from the perlthrtut, 2010.11.20
    #
    # NOTE: There was another older Perl threading flavor called the
    # 5.005 model that used the Threads class. This old model was
    # known to have problems, is deprecated, and was removed for
    # release 5.10. You are strongly encouraged to migrate any
    # existing 5.005 threads code to the new model as soon as
    # possible.
    my $message = "Using Perl version $version\n" .
                  "\tOld threads style supplanted by ithreads after ".
                  "Perl version 5.10\n";
    print STDERR "$message";
    plan(skip_all => $message);
    exit;
}

if (!eval { require Thread; my $t = Thread->new(sub { }) }) {
    print "1..0\n";
    exit 0;
}

my($handle, $port) = Net::Daemon::Test->Child
    ($numTests, $^X, 't/server', '--timeout', 20, '--mode=threads');


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
	if (!$fh->print("$i\n")  ||  !$fh->flush()) {
	    die "Error while writing $i: " . $fh->error() . " ($!)";
	}

	my $line = $fh->getline();
	die "Error while reading $i: " . $fh->error() . " ($!)\n"
	    unless defined($line);
	die "Result error: Expected " . ($i*2) . ", got $line"
	    unless ($line =~ /(\d+)/  &&  $1 == $i*2);
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
}
