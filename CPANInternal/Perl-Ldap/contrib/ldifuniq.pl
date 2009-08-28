#! /usr/bin/perl

# $Id: ldifuniq.pl,v 1.1 2002/08/24 15:11:21 kartik_subbarao Exp $

=head1 NAME

ldifuniq.pl - Culls unique entries from a reference file with respect to a
comparison file.

=head1 DESCRIPTION

ldifuniq.pl takes as input two LDIF files, a reference file and a comparison
file. Each entry in the reference file is compared to its counterpart in the 
comparison file. If it does not have a counterpart, or if the counterpart is 
not identical, the reference entry is printed to standard output. Otherwise no 
output is generated. This behavior is analogous to the -u option of the uniq 
command.

=head1 SYNOPSIS

ldifuniq.pl reffile.ldif cmpfile.ldif

=head1 AUTHOR

Kartik Subbarao E<lt>subbarao@computer.orgE<gt>

=cut


use MIME::Base64;

use strict;


my $reffile = $ARGV[0];
my $cmpfile = $ARGV[1];

die "usage: $0 reffile cmpfile\n" unless $reffile && $cmpfile;

$/ = "";


sub getdn {
	my $rec = shift;
	my $dn;

	1 while s/^(dn:.*)?\n /$1/im; # Handle line continuations
	if (/^dn(::?) (.*)$/im) {
		$dn = $2;
		$dn = decode_base64($dn) if $1 eq '::';
	}

	$dn;
}

open(CMPFH, $cmpfile) || die "$cmpfile: $!\n";
my (%cmpdnpos, $pos); $pos = 0;
while (<CMPFH>) {
	my $dn = getdn($_);
	$cmpdnpos{$dn} = $pos;
	$pos = tell;
}

open(REFFH, $reffile) || die "$reffile: $!\n";
while (<REFFH>) {
	my $refrec = $_; $refrec .= "\n" if $refrec !~ /\n\n$/;
	my $dn = getdn($refrec);
	my $pos = $cmpdnpos{$dn};
	if ($pos eq undef) { 
		print $refrec; next; # Not in cmpfile, print the entry.
	}
	seek(CMPFH, $pos, 0);
	my $cmprec = <CMPFH>; $cmprec .= "\n" if $cmprec !~ /\n\n$/;
	print $refrec if $refrec ne $cmprec;
}
