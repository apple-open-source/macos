# -*- perl -*-
#

require 5.004;
use strict;
use IO::Socket ();
use Config ();
use Net::Daemon::Test ();
use Fcntl ();
use Config ();
use POSIX qw/WNOHANG/;

my $debug = 0;
my $dh;
if ($debug) {
    $dh = Symbol::gensym();
    open($dh, ">", "forkm.log") or die "Failed to open forkm.log: $!";
}

sub log($) {
    my $msg = shift;
    print $dh "$$: $msg\n" if $dh;
}

&log("Start");
my $ok;
eval {
  if ($^O ne "MSWin32") {
    my $pid = fork();
    if (defined($pid)) {
      if (!$pid) { exit 0; } # Child
    }
    wait;
    $ok = 1;
  }
};
if (!$ok) {
  &log("!ok");
  print "1..0\n";
  exit;
}


$| = 1;
$^W = 1;


my($handle, $port);
if (@ARGV) {
    $port = shift @ARGV;
} else {
    ($handle, $port) = Net::Daemon::Test->Child
	(10, $^X, '-Iblib/lib', '-Iblib/arch', 't/server',
	 '--mode=fork', 'logfile=stderr', 'debug');
}


sub IsNum {
    my $str = shift;
    (defined($str)  &&  $str =~ /(\d+)/) ? $1 : undef;
}


sub ReadWrite {
    my $fh = shift; my $i = shift; my $j = shift;
    &log("ReadWrite: -> fh=$fh, i=$i, j=$j");
    if (!$fh->print("$j\n")  ||  !$fh->flush()) {
	die "Child $i: Error while writing $j: " . $fh->error() . " ($!)";
    }
    my $line = $fh->getline();
    &log("ReadWrite: line=$line");
    die "Child $i: Error while reading: " . $fh->error() . " ($!)"
	unless defined($line);
    my $num;
    die "Child $i: Cannot parse result: $line"
	unless defined($num = IsNum($line));
    die "Child $i: Expected " . ($j*2) . ", got $num"
	unless $j*2 == $num;
    &log("ReadWrite: <-");
}


sub MyChild {
    my $i = shift;

    &log("MyChild: -> $i");

    eval {
	my $fh = IO::Socket::INET->new('PeerAddr' => '127.0.0.1',
				       'PeerPort' => $port);
	if (!$fh) {
	    &log("MyChild: Cannot connect: $!");
	    die "Cannot connect: $!";
	}
	for (my $j = 0;  $j < 1000;  $j++) {
	    ReadWrite($fh, $i, $j);
	}
    };
    if ($@) {
	print STDERR "Client: Error $@\n";
	&log("MyChild: Client: Error $@");
	return 0;
    }
    &log("MyChild: <-");
    return 1;
}


sub ShowResults {
    &log("ShowResults: ->");
    my @results;
    for (my $i = 1;  $i <= 10;  $i++) {
	$results[$i-1] = "not ok $i\n";
    }
    if (open(LOG, "<log")) {
	while (defined(my $line = <LOG>)) {
	    if ($line =~ /(\d+)/) {
		$results[$1-1] = $line;
	    }
	}
    }
    for (my $i = 1;  $i <= 10;  $i++) {
	print $results[$i-1];
    }
    &log("ShowResults: <-");
    exit 0;
}

my %childs;
sub CatchChild {
    &log("CatchChild: ->");
    for(;;) {
	my $pid = waitpid -1, WNOHANG;
        last if $pid <= 0;
	if ($pid > 0) {
	    &log("CatchChild: $pid");
	    if (exists $childs{$pid}) {
		delete $childs{$pid};
                if (keys(%childs) == 0) {
                    # We ae done when the last of our ten childs are gone.
                    ShowResults();
                    last;
		}
	    }
	}
    }
    $SIG{'CHLD'} = \&CatchChild;
    &log("CatchChild: <-");
}
$SIG{'CHLD'} = \&CatchChild;

# Spawn 10 childs, each of them running a series of test
unlink "log";
&log("Spawning childs");
for (my $i = 0;  $i < 10;  $i++) {
    if (defined(my $pid = fork())) {
	if ($pid) {
	    # This is the parent
	    $childs{$pid} = $i;
	} else {
	    &log("Child starting");
	    # This is the child
	    undef $handle;
	    %childs = ();
	    my $result = MyChild($i);
	    my $fh = Symbol::gensym();
	    if (!open($fh, ">>log")  ||  !flock($fh, 2)  ||
		!seek($fh, 0, 2)  ||
		!(print $fh (($result ? "ok " : "not ok "), ($i+1), "\n"))  ||
		!close($fh)) {
		print STDERR "Error while writing log file: $!\n";
		exit 1;
	    }
	    exit 0;
	}
    } else {
	print STDERR "Failed to create new child: $!\n";
	exit 1;
    }
}

my $secs = 120;
while ($secs > 0) {
    $secs -= sleep $secs;
}

END {
    &log("END: -> handle=" . (defined($handle) ? $handle : "undef"));
    if ($handle) {
	$handle->Terminate();
	undef $handle;
    }
    while (my($var, $val) = each %childs) {
	kill 'TERM', $var;
    }
    %childs = ();
    unlink "ndtest.prt";
    &log("END: <-");
    exit 0;
}
