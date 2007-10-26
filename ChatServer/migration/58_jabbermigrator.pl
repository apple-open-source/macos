#!/usr/bin/perl -w
#
# Copyright 2006 Apple Computer Inc. All rights reserved.
#
# Migrate Jabber Server configuration and data from Tiger Server to Leopard Server
#

require 'Foundation.pm';
use File::Basename;

my $DEBUG = 0;
my $MKTEMP_PATH = "/usr/bin/mktemp";
my $MKDIR_PATH = "/bin/mkdir";
my $jabberSpoolPath = "/var/jabber/spool";
my $jabberConfigPath = "/etc/jabber";
my $versionPath = "/System/Library/CoreServices/ServerVersion.plist";
my $logPath = "/Library/Logs/Migration/jabbermigrator.log";
my $jabberConfigMigrator = "/var/jabberd/migration/jabber_config_migrator.pl";
my $jabberDataMigrator = "/var/jabberd/migration/jabber_data_migrator.rb";
my $jabberDataSelector = "/var/jabberd/migration/jabber_migration_selector.pl";
my $ichatserver_init_tool = "/usr/libexec/ichatserver_init_tool";
my $sqliteDbFile = "/var/jabberd/sqlite/jabberd2.db";
my $originalBackupDir = "/var/jabberd/migration/old";

sub log_message
{
    open(LOGFILE, ">>$logPath") || die "$0: cannot open $logPath: $!\n";
    $time = localtime();
    print LOGFILE "$time: ". basename($0). ": @_\n";
    close(LOGFILE);
}

sub bail
{
	&log_message(@_);
	&log_message("Aborting!");
	exit 1;
}	

############ MAIN
if ( $> != 0 ) {
	&bail(basename($0) . ": Must be run as root.");
}

if ( $#ARGV > -1 ) {
	print(basename($0) . ": Migrates Jabber Server configuration and data from Tiger Server to Leopard Server.\n");
	print("Writes log messages to \"$logPath\".\n");
	exit 1;
}

if (! -d dirname($logPath) ) {
	$logDir = dirname($logPath);
	`$MKDIR_PATH -p $logDir`;
}

if (! -d  $jabberSpoolPath) { 
	&bail("Error: jabberSpoolPath \"$jabberSpoolPath\" does not exist");
}

if (! -d $jabberConfigPath) {
	&bail("Error: jabberConfigPath \"$jabberConfigPath\" does not exist");
}

if (! -s $versionPath) {
	&bail("Error: Missing or empty file at \"$versionPath\".");
}

if (! -e $ichatserver_init_tool) {
	&bail("Error: Can't find ichatserver_init_tool at \"$ichatserver_init_tool\".");
}

if (! -e $jabberConfigMigrator) {
	&bail("Error: Can't find jabberConfigMigrator at \"$jabberConfigMigrator\".");
}

if (! -e $jabberDataMigrator) {
    &bail("Error: Can't find jabberDataMigrator at \"$jabberDataMigrator\".");
} 

if (! -e $jabberDataSelector) {
    &bail("Error: Can't find jabberDataSelector at \"$jabberDataSelector\".");
} 

my $versionPathNS = NSString->stringWithCString_($versionPath);
my $versionDictNS = NSDictionary->dictionaryWithContentsOfFile_($versionPathNS);
if (!$versionDictNS or !$$versionDictNS) {
	&bail("Error: Unable to determine system version. Can't get NSDictionary for \"$versionPath\". Migration not attempted.");
}

my $productVersionKey = "ProductVersion";
my $productVersionKeyNS = NSString->stringWithCString_($productVersionKey);
my $productVersionNS = $versionDictNS->objectForKey_($productVersionKeyNS);
if (!$productVersionNS or !$$productVersionNS) {
	&bail("Error: Unable to determine system version. \"$versionPath\" does not contain a dictionary with key \"$productVersionKey\". Migration not attempted.");
}

my $productVersion = $productVersionNS->cString();
if ( !($productVersion =~ "^10\.5.*")) {
	&bail("Error: Incorrect system version \"$productVersion\". Migration not attempted.");
}

# execute ichatserver_init_tool to initialize database and update the new configs
my $execString = "";
if ($DEBUG) {
	$execString = "$ichatserver_init_tool -d -i";
} else {
	$execString = "$ichatserver_init_tool -i";
}

&log_message("Executing: $execString");
if (system($execString)) {
	&bail("Error: tool return an error status: \"$execString\"");
}

# migrate configs
opendir(DIR, $jabberSpoolPath) || &bail("Error: cannot open jabberSpoolPath at \"$jabberSpoolPath\"");
my @realms = readdir(DIR);
closedir(DIR);

if (system($jabberConfigMigrator)) {
	&bail("Error: The following tool exited with an error status:\n$jabberConfigMigrator");
}
	
# migrate data
my $tmpFileName = "";
foreach my $realm (@realms) {
	if ($realm eq "." || $realm eq "..") {
		next;
	}
	for (my $i = 0; $i < 5; $i++) {
    	$tmpFileName = `$MKTEMP_PATH /tmp/jabber_migration.XXXXXXXXXXXXXXXXXXXXXXXX`;
    	chomp($tmpFileName);
    	if (-e $tmpFileName) {
        	last;
    	}
    	if ($i == 4) {
        	&bail("Error: Cannot create temporary file:\n$tmpFileName");
    	}
	}
	unlink($tmpFileName);

	if ($DEBUG) {
		$execString = "$jabberDataSelector -d -r \"$realm\" -f \"$tmpFileName\"";
	} else {
		$execString = "$jabberDataSelector -r \"$realm\" -f \"$tmpFileName\"";
	}

	&log_message("Executing: $execString");
	if (system($execString)) {
		&bail("Error: The following tool exited with an error status:\n$execString");
	}

	if ($DEBUG) {
		$execString = "$jabberDataMigrator -d \"$tmpFileName\" \"$sqliteDbFile\"";
	} else {
		$execString = "$jabberDataMigrator \"$tmpFileName\" \"$sqliteDbFile\"";
	}

	&log_message("Executing: $execString");
	if (system($execString)) {
		&bail("Error: The following tool exited with an error status:\n$execString");
	}
	unlink($tmpFileName);
}
	
# todo: rename old spool/config dirs
&log_message("Moving old jabber data and config to $originalBackupDir");
`$MKDIR_PATH -p $originalBackupDir/data`;
`$MKDIR_PATH -p $originalBackupDir/config`;
`mv -f $jabberSpoolPath $originalBackupDir/data`;
`mv -f $jabberConfigPath $originalBackupDir/config`;

&log_message("Upgrade completed successfully.");

