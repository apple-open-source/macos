#!/usr/local/bin/perl4
#
# count_pf.perl-- run lsof in repeat mode and count processes and
#		  files

sub interrupt { print "\n"; exit 0; }

$LSOF = "../lsof";			# path to lsof
$RPT = 15;				# lsof repeat time

if ( ! -x $LSOF) { print "can't execute $LSOF\n"; exit 1 }

# Read lsof -nPF output repeatedly from a pipe.

$| = 1;					# unbuffer output
$SIG{'INT'} = 'interrupt';		# catch interrupt
$proc = $files = $proto{'TCP'} = $proto{'UDP'} = 0;
$progress="/";				# used to show "progress"
open(P, "$LSOF -nPF -r $RPT|") || die "can't open pipe to $LSOF\n";

while (<P>) {
    chop;
    if (/^m/) {

    # A marker line signals the end of an lsof repetition.

	printf "%s  Processes: %5d,  Files: %6d,  TCP: %6d, UDP: %6d\r",
	    $progress, $proc, $files, $proto{'TCP'}, $proto{'UDP'};
	$proc = $files = $proto{'TCP'} = $proto{'UDP'} = 0;
	if ($progress eq "/") { $progress = "\\"; } else { $progress = "/"; }
	next;
    }
    if (/^p/) { $proc++; next; }		# Count processes.
    if (/^f/) { $files++; next; }		# Count files.
    if (/^P(.*)/) { $proto{$1}++; next; }	# Count protocols.
}
