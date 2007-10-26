#!/usr/bin/perl
# $XFree86: xc/programs/xterm/vttests/resize.pl,v 1.1 2004/03/04 02:21:58 dickey Exp $
#
# -- Thomas Dickey (2004/3/3)
# resize.sh rewritten into Perl for comparison.
# See also Term::ReadKey.

use IO::Handle;

sub write_tty {
	open TTY, "+</dev/tty" or die("Cannot open /dev/tty\n");
	autoflush TTY 1;
	print TTY @_;
	close TTY;
}

sub get_reply {
	open TTY, "+</dev/tty" or die("Cannot open /dev/tty\n");
	autoflush TTY 1;
	$old=`stty -g`;
	system "stty raw -echo min 0 time 5";

	print TTY @_;
	my $reply=<TTY>;
	close TTY;
	system "stty $old";
	return $reply;
}

sub csi_field {
	my $first = @_[0];
	my $second = @_[1];
	$first =~ s/^[^0-9]+//;
	while ( --$second > 0 ) {
		$first =~ s/^[\d]+//;
		$first =~ s/^[^\d]+//;
	}
	$first =~ s/[^\d]+.*$//;
	return $first;
}

$original=get_reply("\x1b[18t");
if ( $original =~ /\x1b\[8;\d+;\d+t/ ) {
	$high=csi_field($original,2);
	$wide=csi_field($original,3);
	printf "parsed terminal size $high,$wide\n";
} else {
	die "Cannot get terminal size via escape sequence\n";
}
#
$maximize=get_reply("\x1b[19t");
if ( $maximize =~ /\x1b\[9;\d+;\d+t/ ) {
	$maxhigh=csi_field($maximize,2);
	$maxwide=csi_field($maximize,3);
	$maxhigh != 0 or $maxhigh = $high * 2;
	$maxwide != 0 or $maxwide = $wide * 2;
	printf "parsed terminal maxsize $maxhigh,$maxwide\n";
} else {
	die "Cannot get terminal size via escape sequence\n";
}

sub catch_zap {
	$zapped++;
}
$SIG{INT} = \&catch_zap;
$SIG{QUIT} = \&catch_zap;
$SIG{KILL} = \&catch_zap;
$SIG{HUP} = \&catch_zap;
$SIG{TERM} = \&catch_zap;

$w=$wide;
$h=$high;
$a=1;
$zapped=0;
while ( $zapped == 0 )
{
#	sleep 1
	printf "resizing to $h by $w\n";
	write_tty("\x1b[8;$h;$w" . "t");
	if ( $a == 1 ) {
		if ( $w == $maxwide ) {
			$h += $a;
			if ( $h = $maxhigh ) {
				$a = -1;
			}
		} else {
			$w += $a;
		}
	} else {
		if ( $w == $wide ) {
			$h += $a;
			if ( $h = $high ) {
				$a=1;
			}
		} else {
			$w += $a;
		}
	}
}
write_tty($original);
