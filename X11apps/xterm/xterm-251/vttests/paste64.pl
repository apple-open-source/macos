#!/usr/bin/perl -w
# $XTermId: paste64.pl,v 1.9 2006/03/13 01:28:02 tom Exp $
# $XFree86: xc/programs/xterm/vttests/paste64.pl,v 1.1 2006/03/13 01:28:02 dickey Exp $
#
# -- Thomas Dickey (2006/3/7)
# Test the paste64 option of xterm.

use strict;

use Term::ReadKey;
use IO::Handle;
use MIME::Base64;

our $target = "";

sub to_hex($) {
	my $value = $_[0];
	my $result = "";
	my $n;

	for ( $n = 0; $n < length($value); ++$n) {
		$result .= sprintf("%02X", ord substr($value,$n,1));
	}
	return $result;
}

sub show_string($) {
	my $value = $_[0];
	my $n;

	my $result = "";
	for ( $n = 0; $n < length($value); $n += 1) {
		my $c = ord substr($value,$n,1);
		if ( $c == ord '\\' ) {
			$result .= "\\\\";
		} elsif ( $c == 0x1b ) {
			$result .= "\\E";
		} elsif ( $c == 0x7f ) {
			$result .= "^?";
		} elsif ( $c == 32 ) {
			$result .= "\\s";
		} elsif ( $c < 32 ) {
			$result .= sprintf("^%c", $c + 64);
		} elsif ( $c > 128 ) {
			$result .= sprintf("\\%03o", $c);
		} else {
			$result .= chr($c);
		}
	}

	printf "%s\r\n", $result;
}

sub get_reply($) {
	my $command = $_[0];
	my $reply = "";

	printf "send: ";
	show_string($command);

	print STDOUT $command;
	autoflush STDOUT 1;
	while (1) {
		my $test=ReadKey 1;
		last if not defined $test;
		#printf "%d:%s\r\n", length($reply), to_hex($test);
		$reply .= $test;
	}
	return $reply;
}

sub get_paste() {
	my $reply = get_reply("\x1b]52;" . $target . ";?\x1b\\");

	printf "read: ";
	show_string($reply);

	my $data = $reply;
	$data =~ s/^\x1b]52;[[:alnum:]]*;//;
	$data =~ s/\x1b\\$//;
	printf "chop: ";
	show_string($data);

        $data = decode_base64($data);
	printf "data: ";
	show_string($data);
}

sub put_paste() {
	ReadMode 1;

	printf "data: ";
	my $data = ReadLine 0;
	chomp $data;
	ReadMode 5;

	$data = encode_base64($data);
	chomp $data;
	printf "data: ";
	show_string($data);

	my $send = "\x1b]52;" . $target . ";" . $data . "\x1b\\";

	printf "send: ";
	show_string($send);
}

sub set_target() {
	ReadMode 1;

	printf "target: ";
	$target = ReadLine 0;
	$target =~ s/[^[:alnum:]]//g;
	ReadMode 5;
	printf "result: %s\r\n", $target;
}

ReadMode 5, 'STDIN'; # allow single-character inputs
while (1) {
	my $cmd;

	printf "\r\nCommand (? for help):";
	$cmd = ReadKey 0;
	if ( $cmd eq "?" ) {
		printf "\r\np=put selection, g=get selection, q=quit, r=reset target, s=set target\r\n";
	} elsif ($cmd eq "p") {
		printf " ...put selection\r\n";
		put_paste();
	} elsif ($cmd eq "g") {
		printf " ...get selection\r\n";
		get_paste();
	} elsif ($cmd eq "q") {
		printf " ...quit\r\n";
		last;
	} elsif ($cmd eq "r") {
		printf " ...reset\r\n";
		$target = "";
	} elsif ($cmd eq "s") {
		printf " ...set target\r\n";
		set_target();
	}
}
ReadMode 0, 'STDIN'; # Reset tty mode before exiting
