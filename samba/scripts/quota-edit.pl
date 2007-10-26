#! /usr/bin/perl

# Copyright (C) 2006-2007 Apple Inc. All rights reserved.
# edit-quota.pl: automated interface to edquota(8).

use strict;

# Format that edquota emits and expects:
# Quotas for user jpeach:
# /Volumes/FOO: 1K blocks in use: 0, limits (soft = 100, hard = 0)
#        inodes in use: 2, limits (soft = 0, hard = 0)

my $fname;
my $user = $ENV{QUOTA_USER};
my $volume = $ENV{QUOTA_VOLUME};

sub usage
{
    print STDERR <<"EOF";
Usage: edit-quota.pl FILE

Recognized environment variables:
    QUOTA_USER (mandatory)
    QUOTA_VOLUME (mandatory)
    QUOTA_BLOCKS_SOFT
    QUOTA_BLOCKS_HARD
    QUOTA_INODES_SOFT
    QUOTA_INODES_HARD
EOF
}

sub rewind
{
    my $fh = shift;
    seek $fh, 0, $IO::Seekable::SEEK_SET;
}

sub read_quota_record
{
    my $qfile = shift;
    my $rec = { volume => "",
		inodes => { used => 0, hard => 0, soft => 0},
		blocks => { used => 0, hard => 0, soft => 0} };
    my $line;

    rewind $qfile;
    $line = <$qfile>;
    chomp $line;
    unless ($line =~ m/Quotas for user $user:/) {
	die "badly formatted quota record" unless ($line = <$qfile>);
    }

    while (my $line = <$qfile>) {
	chomp $line;

	#print STDOUT "line is $line\n";
	next unless ($line =~ m/1K blocks in use/);

	($rec->{volume},
	 $rec->{blocks}->{used},
	 $rec->{blocks}->{soft},
	 $rec->{blocks}->{hard}) =
	    ($line =~ m/(.+): 1K blocks in use: (\d+), limits \(soft = (\d+), hard = (\d+)\)/);

	# check that this quota rcord is for the volume we are
	# interested in
	if ($volume ne "") {
	    #print STDOUT "matching $volume VS $rec->{volume}\n";
	    next unless ($rec->{volume} =~ m/.*\Q$volume\E.*/);
	    print STDOUT "matched quota record for $volume\n";
	}

	die "badly formatted quota record" unless ($line = <$qfile>);
	($rec->{inodes}->{used},
	 $rec->{inodes}->{soft},
	 $rec->{inodes}->{hard}) =
	    ($line =~ m/inodes in use: (\d+), limits \(soft = (\d+), hard = (\d+)\)/);

	return $rec;
    }
}

sub write_quota_record
{
    my $qfile = shift;
    my $q = shift;

    rewind $qfile;
    truncate $qfile, 0;

    print $qfile <<"EOF";
Quotas for user $user:
$q->{volume}: 1K blocks in use: $q->{blocks}->{used}, limits (soft = $q->{blocks}->{soft}, hard = $q->{blocks}->{hard})
        inodes in use: $q->{inodes}->{used}, limits (soft = $q->{inodes}->{soft}, hard = $q->{inodes}->{hard})
EOF
}

unless ($fname = shift and -w $fname and $user) {
	usage();
	exit 1;
}

open my $qfile, "+<$fname" or die "failed to open $fname: $!";

my $quota = read_quota_record($qfile);
die("no quota record for $user") unless($quota);

if ($ENV{QUOTA_BLOCKS_SOFT} ne "") {
    $quota->{blocks}->{soft} = $ENV{QUOTA_BLOCKS_SOFT};
}
if ($ENV{QUOTA_BLOCKS_HARD} ne "") {
    $quota->{blocks}->{hard} = $ENV{QUOTA_BLOCKS_HARD};
}
if ($ENV{QUOTA_INODES_SOFT} ne "") {
    $quota->{inodes}->{soft} = $ENV{QUOTA_INODES_SOFT};
}
if ($ENV{QUOTA_INODES_HARD} ne "") {
    $quota->{inodes}->{hard} = $ENV{QUOTA_INODES_HARD};
}

write_quota_record($qfile, $quota);

# Dump the file we wrote to make sure we did the right thing.
rewind $qfile;
while (my $line = <$qfile>) {
    print STDOUT $line;
}
close $qfile;

exit 0
