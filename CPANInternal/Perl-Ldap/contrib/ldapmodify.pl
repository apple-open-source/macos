#! /usr/bin/perl

# $Id: ldapmodify.pl,v 1.1 2001/10/23 15:07:41 gbarr Exp $

=head1 NAME 

ldapmodify.pl - A (simplified) ldapmodify clone written in Perl.

=head1 DESCRIPTION

ldapmodify.pl is a simplified ldapmodify clone written in Perl.

=head1 SYNOPSIS

ldapmodify.pl [B<-a>] [B<-c>] [B<-e errors>] [B<-f file>] [B<-D binddn>] 
[B<-w passwd>] [B<-h ldaphost>] [B<-p port>]

The options have the same meaning as those for the standard ldapmodify command.

=cut

use Net::LDAP;
use Net::LDAP::LDIF;

use Getopt::Std;
use IO::File;

use vars qw(%opt);
use strict;

getopts('acD:e:f:h:p:P:w:', \%opt);
$opt{h} ||= 'localhost';
my $conn = Net::LDAP->new($opt{h}) or die "$opt{h}: $!\n";
my $result = $conn->bind($opt{D}, password => $opt{w});
$result->code && die("$opt{h}: bind: ", $result->error, "\n");
my $ldif = Net::LDAP::LDIF->new($opt{f}, "r");
$ldif->{changetype} = 'add' if $opt{a};
my $ldiferr;

while (my $change = $ldif->read_entry()) {
	print "dn: ", $change->dn, "\n";
	my $result = $change->update($conn);
	if ($result->code) {
		print STDERR "ldapmodify: ", $result->error, "\n";
		if ($opt{e}) {
			if (!$ldiferr) {
				$ldiferr = Net::LDAP::LDIF->new($opt{e}, 'a', change => 1) 
					or die "$opt{e}: $!\n";
			}
			print { $ldiferr->{fh} } "# Error: ", $result->error;
			$ldiferr->write_entry($change);
			print { $ldiferr->{fh} } "\n";
		}
		last unless $opt{c};
	}
	print "\n";
}

=head1 AUTHOR

Kartik Subbarao <subbarao@computer.org>

=cut
