#! /usr/bin/perl
# $Id: ldifsort.pl,v 1.9 2005/04/03 20:20:24 subbarao Exp $

=head1 NAME

ldifsort.pl - Sorts an LDIF file by the specified key attribute. The sorted
version is written to standard output.

=head1 DESCRIPTION

Sorts an LDIF file by the specified key attribute. 

=head1 SYNOPSIS

ldifsort.pl B<-k keyattr> [B<-andc>] file.ldif

=over 4

=item B<-k>

Specifies the key attribute for making sort comparisons. If 'dn' is
specified, sorting is done by the full DN string, which can be composed of 
different attributes for different entries.

=item B<-a>

Specifies that attributes within a given entry should also be sorted. This
has the side effect of removing all comments and line continuations in the 
LDIF file.

=item B<-n>

Specifies numeric comparisons on the key attribute. Otherwise string
comparisons are done.

=item B<-d>

Specifies that the key attribute is a DN. Comparisons are done on a
DN-normalized version of attribute values. This is the default 
behavior if 'dn' is passed as the argument to B<-k>.

=item B<-c>

Specifies case-insensitive comparisons on the key attribute. This is the
default behavior if 'dn' is passed as the argument to B<-k>.

=back


=head1 AUTHOR

Kartik Subbarao E<lt>subbarao@computer.orgE<gt>

=cut


use Net::LDAP::Util qw(canonical_dn);
use MIME::Base64;
use Getopt::Std;

use strict;

my %args;
getopts("k:andc", \%args);

my $keyattr = $args{k};
my $sortattrs = $args{a};
my $ciscmp = $args{c};
my $ldiffile = $ARGV[0];

die "usage: $0 -k keyattr [-n] [-d] [-c] ldiffile\n"
	unless $keyattr && $ldiffile;

$/ = "";

open(LDIFH, $ldiffile) || die "$ldiffile: $!\n";

my $pos = 0;
my @valuepos;
while (<LDIFH>) {
	my $value;
	1 while s/^($keyattr:.*)?\n /$1/im; # Handle line continuations
	if (/^$keyattr(::?) (.*)$/im) {
		$value = $2;
		$value = decode_base64($value) if $1 eq '::';
	}
	$value = lc($value) if $ciscmp;
	push @valuepos, [ $value, $pos ];
	$pos = tell;
}

sub cmpattr { $a->[0] cmp $b->[0] }
sub cmpattrnum { $a->[0] <=> $b->[0] }
my %canonicaldns;
sub cmpdn { 
	my $cadn = ($canonicaldns{$a->[0]} ||= lc(canonical_dn($a->[0])));
	my $cbdn = ($canonicaldns{$b->[0]} ||= lc(canonical_dn($b->[0])));
	$cadn cmp $cbdn;
}

my $cmpfunc;
if ($args{d} || lc($keyattr) eq 'dn') { $cmpfunc = \&cmpdn }
elsif ($args{n}) { $cmpfunc = \&cmpattrnum }
else { $cmpfunc = \&cmpattr; }

my @sorted;
@sorted = sort $cmpfunc @valuepos;

foreach my $valuepos (@sorted) {
	seek(LDIFH, $valuepos->[1], 0);
	my $entry = <LDIFH>;
	if ($sortattrs) {
		$entry =~ s/\n //mg; # collapse line continuations
		my @lines = grep(!/^#/, split(/\n/, $entry));
		my $dn = shift(@lines);
		print "$dn\n", join("\n", sort @lines), "\n\n";
	}
	else {
		print $entry;
		print "\n" if $entry !~ /\n\n$/;
	}
}
