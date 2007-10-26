#!/usr/bin/perl -w

use strict;
use IO::File;

###
# Read in services entries in the format of the file
# located at:
#
# http://www.iana.org/assignments/port-numbers
#
# and merge new entries into the existing services file
###

if (!defined($ARGV[0]) || !defined($ARGV[1])) {
	die "usage: update-services.pl services port-numbers\n";
}

sub parse_services {
	my $file = shift;
	my $names = shift;
	my $descs = shift;
	my $emails = shift;

	my $service = qr/[a-zA-Z0-9_+\/.*-]+/;
	my $protocol = qr/[0-9]+\/[ut][dc]p/;
	my $description = qr/.*/;

	my $prevserv;

	my $handle = new IO::File;
	open($handle, $file) or die "$file: $!";
	while (<$handle>) {
		### Capture lines that look like service entires and
		### add them to the hash tables.
		if (m/^#?\s*($service)\s+($protocol)\s+($description)$/) {
			my $key = $2;
			my $str = $3;

			$prevserv = $key;

			$names->{$key} = $1;
			$str =~ s/\x0d//g;
			chomp($str);
			$descs->{$key} = $str;
		### Capture the email address line that immediately
		### follows service entries.
		} elsif (defined($prevserv) && m/^#\s+([^ ]+)\s*$/) {
			my $str = $1;
			$str =~ s/\x0d//g;
			chomp($str);
			$emails->{$prevserv} = $str;
			$prevserv = undef;
		}
	}
	close($handle);
}

my %iana_names = ();
my %iana_descs = ();
my %iana_emails = ();
my %local_names = ();
my %local_descs = ();
my %local_emails = ();

&parse_services($ARGV[0], \%local_names, \%local_descs, \%local_emails);
&parse_services($ARGV[1], \%iana_names, \%iana_descs, \%iana_emails);

###
### Utility functions for parsing and sorting protocol fields (i.e. 1234/tcp).
###
sub protonum {
	my ($a) = split(/\//, shift);
	return $a;
}

sub _cmpproto {
	my $a = shift;
	my $b = shift;
	my $res = protonum($a) <=> protonum($b);
	if ($res == 0) {
		$res = $a cmp $b;
	}
	return $res;
}
sub cmpproto {
	return &_cmpproto($a, $b);
}

###
### Find services recently added by IANA
###
my @additions = ();
foreach my $key (sort cmpproto keys(%iana_names)) {
	if (not exists $local_names{$key}) {
		push @additions, $key;
	}
}

###
### Find services recently deleted by IANA
###
my @deletions = ();
foreach my $key (sort cmpproto keys(%local_names)) {
	if (not exists $iana_names{$key}) {;
		push @deletions, $key;
	}
}

###
### Find services whose local definition conflicts with IANA
###
my @conflicts = ();
foreach my $key (sort cmpproto keys(%local_names)) {
	if (exists $iana_names{$key} &&
	    exists $local_names{$key} &&
	    $iana_names{$key} ne $local_names{$key}) {
		push @conflicts, $key;
	}
}

###
### Merge Services
###
my $service = qr/[a-zA-Z0-9_+\/.*-]+/;
my $protocol = qr/[0-9]+\/[ut][dc]p/;
my $description = qr/.*/;

my $prev_add;
my $next_add = shift @additions;

my $prev_del;
my $next_del = shift @deletions;

my $handle = new IO::File;
open($handle, $ARGV[0]) or die "$ARGV[0]: $!";
my $line = <$handle>;

###
### Read the existing services file, and process each line.
### Walk the list of additions and deletions, inserting new service
### entries and suppressing existing entries in the output.
###
while (defined($next_add) && defined($line)) {
	if ($line =~ m/^#?\s*($service)\s+($protocol)\s+($description)$/) {
		my $proto = $2;

		my $res;

		### Deletions (replace with Unassigned)
		if (undef) {
			print "#               ";
			print sprintf "% -11s ", protonum($next_del);
			print "Unassigned\n";
			$prev_del = $next_del;
			$next_del = shift @deletions;
			$line = <$handle>;
			next;
		}

		### Additions
		$res = &_cmpproto($next_add, $proto);
		if ($res == 1) {
			###
			### Output Unassigned comment if necessary
			###
			if (defined($prev_add) && _cmpproto($prev_add, $proto) == -1) {
				my $start = &protonum($prev_add) + 1;
				my $end = &protonum($proto) - 1;
				print "#               ";
				if ($start < $end) {
					print sprintf "% -11s ", "$start-$end";
				} else {
					print sprintf "% -11s ", "$start";
				}
				print "Unassigned\n";
				$prev_add = undef;
			}
			print "$line";
			$line = <$handle>;
			next;
		} elsif ($res == 0) {
			# XXX conflict
			die "conflicting entry: $next_add";
			print "$line";
			$line = <$handle>;
		} elsif ($res == -1) {
			if (defined($prev_add)) {
				###
				### Update Unassigned Range (if applicable)
				###
				my $start = &protonum($prev_add);
				my $end = &protonum($next_add);
				if (($end - $start) > 1) {
					++$start;
					--$end;
					print "#               ";
					if ($start < $end) {
					print sprintf "% -11s ", "$start-$end";
					} else {	
					print sprintf "% -11s ", "$start";
					}
					print "Unassigned\n";
				}
			}

			###
			### Print the new IANA entry (addition to file)
			###
			print sprintf "% -15s ", $iana_names{$next_add};
			print sprintf "% -11s ", $next_add;
			print "# ". $iana_descs{$next_add} if exists $iana_descs{$next_add};
			print "\n";
			###
			### Print email address / other comment after new entry
			###
			if (exists $iana_emails{$next_add}) {
			print "#                          ";
			print $iana_emails{$next_add};
			print "\n";
			}
			$prev_add = $next_add;
			$next_add = shift @additions;
		}
	} elsif ($line =~ m/^#\s+([0-9]+)-?([0-9]*)\s+Unassigned.*$/) {
		if ($1 != &protonum($next_add)) {
			print "$line";
		}
		$line = <$handle>;
	} else {
		print "$line";
		$line = <$handle>;
	}
}
close($handle);
