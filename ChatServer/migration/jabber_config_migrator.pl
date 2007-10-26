#!/usr/bin/perl
# Copyright 2006 Apple Computer Inc. All rights reserved.

use Getopt::Std;
use File::Copy;
use File::Basename;

my $DEBUG = 0;
my $SERVERMGR_JABBER_PATH = "/usr/share/servermgrd/cgi-bin/servermgr_jabber";
my $MKTEMP_PATH = "/usr/bin/mktemp";
my $logPath = "/Library/Logs/Migration/jabbermigrator.log";
my $MKDIR_PATH = "/bin/mkdir";

sub usage {
	print "This script reads a jabberd 1.x config XML and migrates the config data into jabberd 2\n";
	print "configs if there are multiple realms in the original XML.\n";
	print "It uses servermgr_jabber to perform the necessary changes to jabberd2 config files.\n\n";
	print "Usage:  jabber_config_migrator.pl [-d] [-c filename]";
	print "Flags:\n";
	print " -d: Debug mode.\n";
	print " -c: Source jabberd 1.x config file. Default is \"/etc/jabber/jabber.xml\".\n";
	print " -?, -h: Show usage info.\n";
}

sub log_message
{
    open(LOGFILE, ">>$logPath") || die "$0: cannot open $logPath: $!";
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
    $logDir = dirname($logPath);
    `$MKDIR_PATH -p $logDir`;
}

getopts('dc:?h', \%opts);

if (defined $opts{'?'} || defined $opts{'h'}) {
	&usage;
	exit 0;
}

if (defined $opts{'d'}) {
	$DEBUG = 1;
}

my $source_config = "";
if (defined $opts{'c'} && $opts{'c'} ne "") {
	$source_config = $opts{'c'};
} else {
	$source_config = "/etc/jabber/jabber.xml";
}

open(IN, "<$source_config") || &bail("ERROR: Cannot open source config $source_config: $!");
my @lines = <IN>;
chomp(@lines);
close(IN);

# parse out original service.host elements to get our realms
my @realms;
my $realm;
my $in_service_elem = 0;

foreach my $line (@lines) {
	if ($line =~ /<service id=\"sessions\">/) {
		&log_message("Found service tag in $source_config") if $DEBUG;
		$in_service_elem = 1;
	}
	if ($line =~ /<host>([a-zA-Z0-9.]+)<\/host>/ && $in_service_elem) {
		&log_message("Found host tag in $source_config: $1") if $DEBUG;
		push(@realms, $1);
	}
	if ($line =~ /<\/service>/) {
		$in_service_elem = 0;
	}
}

if ($#realms < 0) {
	&log_message("No realms to migrate. Exiting.");
	exit 0;
}

my $tmpname;
for ($i = 0; $i < 5; $i++) {
	$tmpname = `$MKTEMP_PATH /tmp/jabber_migration.XXXXXXXXXXXXXXXXXXXXXXXX`;
	chomp($tmpname);
	if (-e $tmpname) {
		last;
	}
	if ($i == 4) {
		&bail("ERROR: Cannot create temporary file: $tmpname");
	}
}

open(OUT, ">$tmpname") || &bail("ERROR: Could not open file $tmpname: $!"); 
print OUT <<"EOF";
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>command</key>
    <string>writeSettings</string>
    <key>configuration</key>
    <dict>
        <key>hosts</key>
        <array>
EOF
foreach $realm (@realms) {
	print OUT  "          <string>$realm</string>\n";
}
print OUT <<"EOF";
        </array>
    </dict>
</dict>
</plist>
EOF

&log_message("Importing settings from file:");
my $exec_string = "$SERVERMGR_JABBER_PATH < $tmpname";
my $res = `$exec_string`;
unlink($tmpname);
&log_message("New servermgr_jabber settings:\n$res");
&log_message("Upgrade completed successfully.");
