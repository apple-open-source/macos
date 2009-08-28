#! /usr/bin/perl
# $Id: ldifdiff.pl,v 3.7 2005/03/15 14:22:45 subbarao Exp $

=head1 NAME

ldifdiff.pl -- Generates LDIF change diff between two sorted LDIF files.

=head1 DESCRIPTION

ldifdiff.pl takes as input two sorted LDIF files, source and target, and 
generates on standard output the LDIF changes needed to transform the target 
into the source.

=head1 SYNOPSIS

ldifdiff.pl B<-k|--keyattr keyattr> [B<-a|--sourceattrs attr1,attr2,...>] [B<-c|--ciscmp attr1,...>] [B<--dnattrs attr1,...>] [B<--sharedattrs attr1,...>] B<sourcefile> B<targetfile>

=head1 OPTIONS

=over 4

=item B<-k|--keyattr> keyattr

Specifies the key attribute to use when comparing source and target entries. 
Entries in both LDIF files must be sorted by this attribute for comparisons to 
be meaningful. F<ldifsort.pl> can be used to sort LDIF files by a given 
attribute.

=item B<-a|--sourceattrs attr1,attr2,...>

(Optional) Specifies a list of attributes to consider when comparing
source and target entries. By default, all attributes are considered.

=item B<-c|--ciscmp attr1,...>

(Optional) Compare values of the specified attributes case-insensitively. The 
default set is: mail manager member objectclass owner uid uniqueMember

=item B<--dnattrs attr1,...>

(Optional) Specifies a list of attributes to be treated as DNs when being
compared. The default set is: manager member owner uniqueMember

=item B<--sharedattrs attr1,...>

(Optional) Specifies a list of attribues to be treated as "shared" attributes,
where the source may not be a sole authoritative source. When modifying
these attributes, separate "delete" and "add" LDIF changes are generated, 
instead of a single "replace" change. The default set is objectClass.

=item B<sourcefile>

Specifies the source LDIF file.

=item B<targetfile>

Specifies the target LDIF file.

=back

=cut

use Net::LDAP;
use Net::LDAP::LDIF;
use Net::LDAP::Util qw(canonical_dn);
use Getopt::Long;

use strict;

my @sourceattrs;
my (%ciscmp, %dnattrs, %sharedattrs);
my $keyattr;
GetOptions('a|sourceattrs=s' => sub { @sourceattrs = split(/,/, $_[1]) },
	'c|ciscmp=s' => sub { my @a = split(/,/,lc $_[1]); @ciscmp{@a} = (1) x @a },
	'dnattrs=s' => sub { my @a = split(/,/,lc $_[1]); @dnattrs{@a} = (1) x @a },
	'k|keyattr=s' => \$keyattr,
	'sharedattrs=s' => sub {my @a=split(/,/,lc $_[1]);@sharedattrs{@a}=(1) x @a}
	);
unless (keys %ciscmp) {
	foreach (qw(mail manager member objectclass owner uid uniquemember)) 
	{ $ciscmp{$_} = 1 }
}
unless (keys %dnattrs) {
	foreach (qw(manager member owner uniquemember))
	{ $dnattrs{$_} = 1 }
}
%sharedattrs = (objectclass => 1)
	unless keys %sharedattrs;


my ($sourcefile, $targetfile);
$sourcefile = shift; $targetfile = shift;

die "usage: $0 -k|--keyattr keyattr [-a|--sourceattrs attr1,attr2,...] [-c|--ciscmp attr1,...] [--dnattrs attr1,...] [--sharedattrs attr1,...] sourcefile targetfile\n"
	unless $keyattr && $sourcefile && $targetfile;

my $source = Net::LDAP::LDIF->new($sourcefile)
	or die "Can't open LDIF file $sourcefile: $!\n";

my $target = Net::LDAP::LDIF->new($targetfile)
	or die "Can't open LDIF file $targetfile: $!\n";

my $ldifout = Net::LDAP::LDIF->new('-', 'w');
$ldifout->{change} = 1;
$ldifout->{wrap} = 78;

diff($source, $target);
exit;


# Gets the relative distinguished name (RDN) attribute
sub rdnattr { ($_[0] =~ /^(.*?)=/)[0] }

# Gets the relative distinguished name (RDN) value
sub rdnval { my $rv = ($_[0] =~ /=(.*)/)[0]; $rv =~ s/(?<!\\),.*//; $rv }

# Gets the rest of the DN (the part after the RDN)
sub dnsuperior { my $rv = ($_[0] =~ /^.*?(?<!\\),(.*)/)[0]; $rv }

sub cmpDNs
{
	my ($adn, $bdn) = @_;
	my $cadn = canonical_dn($adn, casefold => 'lower'); 
	my $cbdn = canonical_dn($bdn, casefold => 'lower');
	if ($ciscmp{lc rdnattr($cadn)}) { $cadn = lc($cadn), $cbdn = lc($cbdn) }

	$cadn cmp $cbdn;
}

sub cmpEntries
{ 
	my ($a, $b) = @_;
	my $dncmp = cmpDNs($a->dn, $b->dn);

	if (lc($keyattr) eq 'dn') { 
		return ($dncmp, $dncmp);
	}
	else { 
		my $aval = $a->get_value($keyattr);
		my $bval = $b->get_value($keyattr);
		if ($ciscmp{$keyattr}) {
			$aval = lc($aval);
			$bval = lc($bval);
		}
		return($aval cmp $bval, $dncmp);
	}
}


# Diffs two LDIF data sources
sub diff
{ 
	my ($source, $target) = @_;
	my ($sourceentry, $targetentry, $incr_source, $incr_target, @ldifchanges);

	$sourceentry = $source->read_entry();
	$targetentry = $target->read_entry();

	while () {
		# End of all data
		last if !$sourceentry && !$targetentry;

		# End of source data, but more target data. Delete.
		if (!$sourceentry && $targetentry) {
			$targetentry->delete;
			$ldifout->write_entry($targetentry);
			$incr_target = 1, next;
		}

		# End of target data, but more data in source. Add.
		if ($sourceentry && !$targetentry) {
			$ldifout->write_entry($sourceentry);
            $incr_source = 1, next;
		}

		my ($entrycmp, $dncmp) = cmpEntries($sourceentry, $targetentry);

		# Check if the current source entry has a higher sort position than 
		# the current target. If so, we interpret this to mean that the 
		# target entry no longer exists on the source. Issue a delete to LDAP.
		if ($entrycmp > 0) {
			$targetentry->delete;
			$ldifout->write_entry($targetentry);
            $incr_target = 1, next;
		}
		# Check if the current source entry has a lower sort position than 
		# the current target entry. If so, we interpret this to mean that the
		# source entry doesn't exist on the target. Issue an add to LDAP.
		elsif ($entrycmp < 0) {
			$ldifout->write_entry($sourceentry);
            $incr_source = 1, next;
		}

		# When we get here, we're dealing with the same person in $sourceentry
		# and $targetentry. Compare the data and generate the update.

		# If a mod{R}DN is necessary, it needs to happen before other mods
		if ($dncmp) {
			my $rdnattr = rdnattr($sourceentry->dn);
			my $rdnval = rdnval($sourceentry->dn);
			my $newsuperior = dnsuperior($sourceentry->dn);
			my $oldsuperior = dnsuperior($targetentry->dn);
			my $changetype; 

			if (cmpDNs($oldsuperior, $newsuperior)) {
				$changetype = 'moddn';
				$targetentry->add(newsuperior => $newsuperior);
			}
			else { $changetype = 'modrdn' }
			$targetentry->{changetype} = $changetype;
			$targetentry->add(newrdn => "$rdnattr=$rdnval",
							  deleteoldrdn => '1');
			$ldifout->write_entry($targetentry);
			$targetentry->delete('newrdn');
			$targetentry->delete('deleteoldrdn');
			$targetentry->delete('newsuperior') if $changetype eq 'moddn';
			delete($targetentry->{changetype});
			
			$targetentry->dn($sourceentry->dn);
			$targetentry->replace($rdnattr, $sourceentry->get_value($rdnattr))
				if $sourceentry->exists($rdnattr);
		}

		# Check for differences and generate LDIF as appropriate
		updateFromEntry($sourceentry, $targetentry, @sourceattrs);
		$ldifout->write_entry($targetentry) if @{$targetentry->{changes}};
		$incr_source = 1, $incr_target = 1, next;

    } continue {
		if ($incr_source) {
			$sourceentry = $source->read_entry(); $incr_source = 0;
		}
		if ($incr_target) {
			$targetentry = $target->read_entry(); $incr_target = 0;
		}
    }
}

# Generate LDIF to update $target with information in $source.
# Optionally restrict the set of attributes to consider.
sub updateFromEntry
{
	my ($source, $target, @attrs) = @_;
	my ($attr, $val, $ldifstr);

	unless (@attrs) {
		# add all source entry attributes
		@attrs = $source->attributes;
		# add any other attributes we haven't seen from the target entry
		foreach my $tattr ($target->attributes) { 
			push(@attrs, $tattr) unless grep(/^$tattr$/i, @attrs) 
		}
	}

	$target->{changetype} = 'modify';

	foreach $attr (@attrs) {
		my $lcattr = lc $attr;
		next if $lcattr eq 'dn'; # Can't handle modrdn here

		# Build lists of unique values in the source and target, to
		# speed up comparisons.
		my @sourcevals = $source->get_value($attr);
		my @targetvals = $target->get_value($attr);
		my (%sourceuniqvals, %targetuniqvals);
		foreach (@sourcevals) {
			my ($origval, $val) = ($_, $_);
			$val = lc $val if $ciscmp{$lcattr};
			# Get rid of spaces after non-escaped commas in DN attrs
			$val =~ s/(?<!\\),\s+/,/g if $dnattrs{$lcattr};
			$sourceuniqvals{$val} = $origval;
		}
		foreach (@targetvals) {
			my ($origval, $val) = ($_, $_);
			$val = lc $val if $ciscmp{$lcattr};
			# Get rid of spaces after non-escaped commas in DN attrs
			$val =~ s/(?<!\\),\s+/,/g if $dnattrs{$lcattr};
			$targetuniqvals{$val} = $origval;
		}
		foreach my $val (keys %sourceuniqvals) {
			if (exists $targetuniqvals{$val}) {
				delete $sourceuniqvals{$val};
				delete $targetuniqvals{$val};
			}
		}

		# Move on if there are no differences
		next unless keys(%sourceuniqvals) || keys(%targetuniqvals);

		# Make changes as appropriate
		if ($sharedattrs{$lcattr}) {
			# For 'shared' attributes (e.g. objectclass) where $source may not 
			# be a sole authoritative source, we issue separate delete and 
			# add modifications instead of a single replace.
			$target->delete($attr => [ values(%targetuniqvals) ])
				if keys(%targetuniqvals);
			$target->add($attr => [ values(%sourceuniqvals) ])
				if keys(%sourceuniqvals);
		}
		else {
			# Issue a replace or delete as needed
			if (@sourcevals) { $target->replace($attr => [ @sourcevals ]) }
			else { $target->delete($attr) }
		}
	}

	# Get rid of the "changetype: modify" if there were no changes
	delete($target->{changetype}) unless @{$target->{changes}};
}


=back

=head1 AUTHOR

Kartik Subbarao E<lt>subbarao@computer.orgE<gt>

=cut

