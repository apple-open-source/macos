#!/usr/bin/perl -w
#
# Copyright 2009 Apple Inc. All rights reserved.
#
# Migrates jabberd 2.0 SQLite database into an initialized jabberd 2.1.24 database
# Changes include: new vcard columns, new status table, and a "NOT NULL" restriction 
# added to the column data types of several tables.

use Getopt::Std;
use File::Basename;
use DBI;
use strict;

# GLOBALS
my $DEBUG = 0;
my $logPath = "/Library/Logs/Migration/jabbermigrator.log";
my $MKDIR_PATH = "/bin/mkdir";
my $BACKUP_DB_PATH = "/var/jabberd/sqlite/jabberd2.db.orig";
my $BACKUP_DB_PERMS = "640";
my $BACKUP_DB_OWNERS = "_jabber:_jabber";
my $CHOWN = "/usr/sbin/chown";
my $CP = "/bin/cp";
my $CHMOD = "/bin/chmod";
my $gBackup;   #  Disabling backup of old db by default

sub usage {
    print "This script reads a jabberd 2.0 SQLite database and migrates the contents into\n";
    print "a jabberd 2.1.24 database.\n";
    print "\n";
    print "Usage:  $0 [-s PATH] [-n PATH] [-d]\n\n";
    print "Flags:\n";
	print " -s [file]  : Source database file (REQUIRED)\n";
	print " -d [file]  : Destination database file (REQUIRED)\n";
    print " -D         : Debug mode.\n";
	print " -b [file]  : Filename to backup original database to [default = Disabled]\n";
    print " -?, -h     : Show usage info.\n";
	exit(0);
}

sub log_message
{
    open(LOGFILE, ">>$logPath") || die "$0: cannot open $logPath: $!";
    my $time = localtime();
    print LOGFILE "$time: ". basename($0). ": @_\n";
    print "@_\n" if $DEBUG;
    close(LOGFILE);
}

sub bail
{
    &log_message(@_);
    &log_message("Aborting!");
	print "@_\n";
    exit 1;
}


##################### MAIN
my $OLD_DB;
my $NEW_DB;
my %opts;

if (! -d dirname($logPath) ) {
    my $logDir = dirname($logPath);
    qx{ $MKDIR_PATH -p $logDir };
	if ($? != 0) {
		&log_message("\"$MKDIR_PATH -p $logDir\" returned failure status $?");
	}
}

getopts('Dd:b:s:?h', \%opts);

if (defined $opts{'?'} || defined $opts{'h'}) {
    &usage;
    exit 0;
}

if (defined $opts{'D'}) {
    $DEBUG = 1;
}

if (defined $opts{'b'}) {
    $gBackup = $opts{'b'};
}

if (defined $opts{'s'}) {
	$OLD_DB = $opts{'s'};
	if ((! -e $OLD_DB) || (! -r $OLD_DB)) {
		&bail("Source database must exist and be readable: $OLD_DB");
	}
} else {
	print "You must specify the source database.\n";
	&usage;
}

if (defined $opts{'d'}) {
	$NEW_DB = $opts{'d'};
	if ((! -e $NEW_DB) || (! -w $NEW_DB)) {
		&bail("Destination database must exist, be initialized, and be writable: $NEW_DB");
	}
} else {
	print "You must specify the destination database.\n";
	&usage;
}

&log_message("Migrating jabberd data from $OLD_DB to $NEW_DB");

# List of each table and its elements to migrate.
# This is very specific to 2.0 -> 2.1.x : Some SQLite columns now specify a "NOT NULL" restriction,
#    so we need to check for NULL in the source data and decide what to do.

my %tables = (
	"active" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "time", 				not_null_added => 1, 	default => 0 },
		],
	},
	"disco-items" => {
		columns => [
			{ col => "jid", 				not_null_added => 0, 	default => '' },
			{ col => "name",				not_null_added => 0, 	default => '' },
			{ col => "node", 				not_null_added => 0,	default => '' },
		],
	},
	"logout" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "time", 				not_null_added => 1, 	default => 0 },
		],
	},
	"motd-message" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0,	default => '' },
			{ col => "xml", 				not_null_added => 1, 	default => '' },
		],
	},
	"motd-times" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "time", 				not_null_added => 1, 	default => '' },
		],
	},
	"privacy-default" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "default", 			not_null_added => 0, 	default => '' },
		],
	},
	"privacy-items" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "list", 				not_null_added => 1, 	default => '' },
			{ col => "type", 				not_null_added => 0, 	default => '' },
			{ col => "value", 				not_null_added => 0, 	default => '' },
			{ col => "deny", 				not_null_added => 0, 	default => '' },
			{ col => "order", 				not_null_added => 0, 	default => '' },
			{ col => "block", 				not_null_added => 0, 	default => '' },
		],
	},
	"private" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "ns", 					not_null_added => 0, 	default => '' },
			{ col => "xml", 				not_null_added => 0, 	default => '' },
		],
	},
	"queue" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "xml", 				not_null_added => 1, 	default => '' },
		],
	},
	"roster-groups" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "jid", 				not_null_added => 1, 	default => '' },
			{ col => "group", 				not_null_added => 1, 	default => '' },
		],
	},
	"roster-items" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "jid", 				not_null_added => 1, 	default => '' },
			{ col => "name", 				not_null_added => 0, 	default => '' },
			{ col => "to", 					not_null_added => 1, 	default => 0 },
			{ col => "from", 				not_null_added => 1, 	default => 0 },
			{ col => "ask", 				not_null_added => 1, 	default => 0 },
		],
	},
	"vacation-settings" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "start", 				not_null_added => 0, 	default => '' },
			{ col => "end", 				not_null_added => 0, 	default => '' },
			{ col => "message", 			not_null_added => 0, 	default => '' },
		],
	},
	"vcard" => {
		columns => [
			{ col => "collection-owner", 	not_null_added => 0, 	default => '' },
			{ col => "fn", 					not_null_added => 0, 	default => '' },
			{ col => "nickname", 			not_null_added => 0, 	default => '' },
			{ col => "url", 				not_null_added => 0, 	default => '' },
			{ col => "tel",					not_null_added => 0, 	default => '' },
			{ col => "email",				not_null_added => 0, 	default => '' },
			{ col => "title",				not_null_added => 0, 	default => '' },
			{ col => "role",				not_null_added => 0, 	default => '' },
			{ col => "bday",				not_null_added => 0, 	default => '' },
			{ col => "desc",				not_null_added => 0, 	default => '' },
			{ col => "n-given",				not_null_added => 0, 	default => '' },
			{ col => "n-family",			not_null_added => 0, 	default => '' },
			{ col => "adr-street",			not_null_added => 0, 	default => '' },
			{ col => "adr-extadd",			not_null_added => 0, 	default => '' },
			{ col => "adr-locality",		not_null_added => 0,	default => '' },
			{ col => "adr-region",			not_null_added => 0, 	default => '' },
			{ col => "adr-pcode",			not_null_added => 0, 	default => '' },
			{ col => "adr-country",			not_null_added => 0, 	default => '' },
			{ col => "org-orgname",			not_null_added => 0, 	default => '' },
			{ col => "org-orgunit",			not_null_added => 0, 	default => '' },
			{ col => "photo-type",			not_null_added => 0, 	default => '' },
			{ col => "photo-binval",		not_null_added => 0, 	default => '' },
		],
	},
);


#backup original database
if (defined($gBackup)) {
	&log_message("Backing up source database to path: $gBackup");
	qx ($CP \"$OLD_DB\" \"$BACKUP_DB_PATH\");
	qx ($CHOWN $BACKUP_DB_OWNERS \"$BACKUP_DB_PATH\");
	qx ($CHMOD $BACKUP_DB_PERMS \"$BACKUP_DB_PATH\");
}

my $commit = 1;
my $dbh_old = DBI->connect("dbi:SQLite:dbname=$OLD_DB", "", "", { RaiseError => 1, AutoCommit => 1 });
my $dbh_new = DBI->connect("dbi:SQLite:dbname=$NEW_DB", "", "", { RaiseError => 1, AutoCommit => 0 });
HANDLE_TABLE: 
for my $table_name (keys %tables) {
	my @tmp_cols;
	my $sql_buf;
	my $num_cols;
	my @dest_vals;
	my $sth_old;
	my $sth_new;
	my $i;	
	my @col = ();
	&log_message("Processing table: $table_name");
	#$num_cols = ($#{@{$tables{$table_name}{columns}}})+1; # fails with strict ref checking
	@tmp_cols = @{$tables{$table_name}{columns}};
	$num_cols = ($#tmp_cols)+1;
	if ($DEBUG) { print "\tDEBUG: num cols = $num_cols\n"; }
	$sql_buf = "SELECT ";
	for ($i = 0; $i < $num_cols; $i++) {
		$sql_buf .= "\`$tables{$table_name}{columns}[$i]{col}\`";
		if ($i != ($num_cols-1)) {
			$sql_buf .= ", ";
		}
	}

	$sql_buf .= " FROM `$table_name`;";
	if ($DEBUG) { print "\tDEBUG: $sql_buf\n"; }
	$sth_old = $dbh_old->prepare( $sql_buf );
	if (! $sth_old->execute()){
		&log_message("ERROR: $sth_old->errstr");
		# abort and rollback on any error
		$commit = 0;
		$sth_old->finish();
		$sth_new->finish();
		last HANDLE_TABLE;
	}

	for ($i = 0; $i < $num_cols; $i++) {
		$sth_old->bind_col( $i+1, \$col[$i]);
	}

	while ($sth_old->fetch()) {
		@dest_vals = ();

		$sql_buf = "INSERT INTO `$table_name` (";
		for ($i = 0; $i < $num_cols; $i++) {
			$sql_buf .= "`$tables{$table_name}{columns}[$i]{col}`";
			if ($i != ($num_cols-1)) {
				$sql_buf .= ", ";
			} else {
				$sql_buf .= ") ";
			}
		}
		$sql_buf .= "VALUES (";
		for ($i = 0; $i <= $#col; $i++) {
			# Check for any NULL values being inserted into a "not null" column
			if ((! defined($col[$i])) && 
				$tables{$table_name}{columns}[$i]{not_null_added}) {
				&log_message("NOTICE: Found NULL in original data, trying to replace with a default value");
				$col[$i] = $tables{$table_name}{columns}[$i]{default};
			}
			$sql_buf .= "?";
			push @dest_vals, $col[$i];
			if ($i != $#col) {
				$sql_buf .= ", ";
			} else {
				$sql_buf .= ");";
			}
		}
		if ($DEBUG) { 
			print "\tDEBUG: Preparing: $sql_buf\n";
			foreach my $temp_val (@dest_vals) {
				if (defined($temp_val)) {
					print "\t\tDEBUG: val = $temp_val\n";
				}
			}
		}
		$sth_new = $dbh_new->prepare($sql_buf);
		if (! $sth_new->execute(@dest_vals)) {
			&log_message("ERROR: $sth_new->errstr");
			# abort and rollback on any error
			$commit = 0;
			$sth_old->finish();
			$sth_new->finish();
			last HANDLE_TABLE;
		}
		$sth_new->finish();
	}
	
	$sth_old->finish();
}
undef $dbh_old;

my $result = ($commit ? $dbh_new->commit : $dbh_new->rollback);
if ((! $result) || (! $commit)) {
	&log_message("Couldn't finish transaction: " . $dbh_new->errstr);
	undef $dbh_new;
	exit(1);
}

undef $dbh_new;
&log_message("Migration succeeded.");