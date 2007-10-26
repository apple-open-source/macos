#!/usr/bin/perl
# Copyright 2006 Apple Computer Inc. All rights reserved.

use Getopt::Std;
use File::Basename;

my $MKDIR_PATH = "/bin/mkdir";
my $logPath = "/Library/Logs/Migration/jabbermigrator.log";

sub usage {
	print "This script creates a list of jabberd 1.x XML data files to be used during migration.\n";
	print "It includes any files which contain a jabber:iq:last element, indicating that the\n";
	print "user has logged in at least once.  Any other files are excluded.\n";
	print "The output is to STDOUT or to a file as specified by the -f flag.\n\n";
	print "Usage:  jabber_migration_selector.pl [-d] [-r realm] [-s spool directory] [-f output file]";
	print "Flags:\n";
	print " -h, -?: Usage info.\n";
	print " -d: Debug mode.\n";
	print " -r <realm> : Realm to use (ie. mychatserver.mydomain.com). Required.\n";
	print " -s <spool directory> : The location of the jabberd spool directory.\n";
	print "    Default is \"/var/jabber/spool\" if unspecified.\n";
	print " -f <output file> : Target file for output.  Defaults to standard out.\n";
}

sub log_message
{
    open(LOGFILE, ">>$logPath") || die "$0: cannot open $logPath: $!\n";
    $time = localtime();
    print LOGFILE "$time: ". basename($0). ": @_\n";
    print "@_\n" if $DEBUG;
    close(LOGFILE);
}

sub bail
{
    &log_message(@_);
    &log_message("Aborting!");
    exit 1;
}


############ MAIN
if (! -d dirname($logPath) ) {
    my $logDir = dirname($logPath);
    `$MKDIR_PATH -p $logDir`;
} 

if ($#ARGV < 0) {
    &log_message("ERROR: Not enough args! Realm is required.");
    &usage;
    exit 1;
}

getopts('dr:s:f:h?', \%opts);

my $DEBUG = 0;
if (defined $opts{'d'}) {
	$DEBUG = 1;
}

if (defined($opts{'?'}) || defined($opts{'h'})) {
	&usage;
	exit 0;
}

my $realm = "";
if (defined $opts{'r'} && $opts{'r'} ne "") {
	$realm = $opts{'r'};
} else {
	&log_message("ERROR: You must specify a realm using the -r flag.");
	&usage;
	exit 1;
}

my $spool_dir = "";
if (defined $opts{'s'} && $opts{'s'} ne "") {
	$spool_dir = $opts{'s'};
} else {
	$spool_dir = "/var/jabber/spool";
}

my $output_file = "";
if (defined $opts{'f'} && $opts{'f'} ne "") {
	$output_file = $opts{'f'};
}

if (! -e "$spool_dir/$realm") {
	&bail("ERROR: directory \"$spool_dir/$realm\" does not exist.");
}

opendir(DIR, "$spool_dir/$realm") || &bail("ERROR: Cannot open spool directory $spool_dir/$realm");
my @files = readdir(DIR);
closedir(DIR);
my $out_count = 0;
my $in_count = 0;
my @files_to_migrate;
my $last;
my $username;
my $spoolFilePath = "";

if ($output_file ne "") {
	 open(OUT, ">$output_file") || &bail("ERROR: Unable to open $output_file");
}

foreach my $file (@files) {
	if ($file !~ /\.xml$/) {
		next;
	}
	$last = 0;
	$username = "";
	$spoolFilePath = "$spool_dir/$realm/$file";
	open(F, "<$spoolFilePath") || &bail("ERROR: Unable to open $spoolFilePath");
	my @lines = <F>;
	close(F);
	chomp(@lines);
	foreach my $line (@lines) {
		if ($line =~ /xmlns=\'jabber:iq:last\' last=\'(\d*)\'/) {
			$last = $1;
		}
	}
	if ($last > 0) {
		if ($output_file ne "") {
			print OUT "$spoolFilePath\n";
		} else {
			print "$spoolFilePath\n";
		}
		&log_message("INCLUDED: $spoolFilePath");
		$in_count++;
	} else {
		&log_message("EXCLUDED: $spoolFilePath");
		$out_count++;
	}
}

if ($output_file ne "") {
	close(OUT);
}

&log_message("TOTAL EXCLUDED: $out_count");
&log_message("TOTAL INCLUDED: $in_count");
