#!/usr/local/bin/perl4
#
# watch_a_file.perl -- use lsof -F output to watch a specific file
#		       (or file system)
#
# usage:	watch_a_file.perl file_name

## Interrupt handler

sub interrupt { wait; print "\n"; exit 0; }


## Start main program

$Pn = "watch_a_file";
# Check file argument.

if ($#ARGV != 0) { print "$#ARGV\n"; die "$Pn usage: file_name\n"; }
$fnm = $ARGV[0];
if (! -r $fnm) { die "$Pn: can't read $fnm\n"; }

# Do setup.

$LSOF = "../lsof";			# path to lsof
$RPT = 15;				# lsof repeat time
$| = 1;					# unbuffer output
$SIG{'INT'} = 'interrupt';		# catch interrupt
if ( ! -x $LSOF) { print "can't execute $LSOF\n"; exit 1 }

# Read lsof -nPF output from a pipe and gather the PIDs of the processes
# and file descriptors to watch.

open(P, "$LSOF -nPFpf $fnm|") || die "$Pn: can't pipe to $LSOF\n";

$curpid = -1;
$pids = "";
while (<P>) {
    chop;
    if (/^p(.*)/) { $curpid = $1; next; }	# Identify process.
    if (/^f/) {
	if ($curpid > 0) {
	    if ($pids eq "") { $pids = $curpid; }
	    else { $pids = $pids . "," . $curpid; }
	    $curpid = -1;
	}
    }
}
close(P);
wait;
if ($pids eq "") { die "$Pn: no processes using $fnm located.\n"; }
print "watch_file: $fnm being used by processes:\n\t$pids\n\n";

# Read repeated lsof output from a pipe and display.

$pipe = "$LSOF -ap $pids -r $RPT $fnm";
open(P, "$pipe|") || die "$Pn: can't pipe: $pipe\n";

while (<P>) { print $_; }
close(P);
print "$Pn: unexpected EOF from \"$pipe\"\n";
exit 1;
