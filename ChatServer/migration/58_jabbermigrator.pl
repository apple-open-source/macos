#!/usr/bin/perl -w
# Copyright 2010 Apple Inc. All rights reserved.
## Migration Script for iChat Server

##################   Input Parameters  #######################
# --purge <0 | 1>	"1" means remove any files from the old system after you've migrated them, "0" means leave them alone.
# --sourceRoot <path>	The path to the root of the system to migrate
# --sourceType <System | TimeMachine>	Gives the type of the migration source, whether it's a runnable system or a 
#                                       Time Machine backup.
# --sourceVersion <ver>	The version number of the old system (like 10.5.x or 10.6). Since we support migration from 10.5, 
#                       10.6, and other 10.7 installs, it's useful to know this information, and it would be easier for me to figure 
#                       it out once and pass it on to each script than to have each script have to figure it out itself.
# --targetRoot <path>	The path to the root of the new system. Pretty much always "/"
# --language <lang> A language identifier, such as \"en.\" Long running scripts should return a description of what they're doing
#                   (\"Migrating Open Directory users\"), and possibly provide status update messages along the way. These messages
#                   need to be localized (which is not necessarily the server running the migration script).
#                   This argument will identify the Server Assistant language. As an alternative to doing localization yourselves
#                   send them in English, but in case the script will do this it will need this identifier.

BEGIN {
	push @INC,"/System/Library/ServerSetup/MigrationExtras/";
}
use MigrationUtilities;
use strict;
use warnings;
use File::Basename;

############################# System  Constants #############################
my $CAT = "/bin/cat";
my $CP = "/bin/cp";
my $DSCL = "/usr/bin/dscl";
my $DU = "/usr/bin/du";
my $ECHO = "/bin/echo";
my $GREP = "/usr/bin/grep";
my $LAUNCHCTL = "/bin/launchctl";
my $MKDIR = "/bin/mkdir";
my $MV = "/bin/mv";
my $PLISTBUDDY = "/usr/libexec/PlistBuddy";
my $SERVERADMIN="/usr/sbin/serveradmin";
my $MUC_TO_ROOMS_DATA_MIGRATOR="/usr/libexec/jabberd/MucDataMigrator";
my $SQLITE3 = "/usr/bin/sqlite3";

################################## Consts ###################################
my $g_jabber_user = "_jabber";
my $g_jabber_group = "_jabber";
my $g_admin_group = "admin";
my $g_jabber_uid = getpwnam($g_jabber_user);
my $g_jabber_gid = getgrnam($g_jabber_group);
my $g_admin_gid = getgrnam($g_admin_group);
my $g_service_name = "jabber";

#################################   PATHS  #################################
my $g_migration_script_path = "/usr/libexec";
my $g_jabber_config_migrator = $g_migration_script_path."/jabber_config_migrator.pl";
my $g_jabber_data_migrator_pre_10_7 = $g_migration_script_path."/jabber_data_migrate_2.0-2.1.pl";
my $g_migration_log_dir = "/Library/Logs/Migration";
my $g_log_path = $g_migration_log_dir."/jabbermigrator.log";
my $g_shared_log_path = "/Library/Logs/Setup.log";
my $g_sqlite_db_path_pre_10_7 = "/private/var/jabberd/sqlite/jabberd2.db";
my $g_sqlite_db_path = "/Library/Server/iChat/Data/sqlite/jabberd2.db";
my $g_bundle_config_path = "/Library/Preferences/com.apple.ichatserver.plist";
my $g_system_plist_path = "/System/Library/CoreServices/SystemVersion.plist";
my $g_server_plist_path = "/System/Library/CoreServices/ServerVersion.plist";
my $g_launchd_config_path_jabberd = "/System/Library/LaunchDaemons/org.jabber.jabberd.plist";
my $g_launchd_config_path_proxy65 = "/System/Library/LaunchDaemons/org.jabber.proxy65.plist";
my $g_launchd_overrides_path = "/private/var/db/launchd.db/com.apple.launchd/overrides.plist";
my $g_rooms_home_sites_path = "/private/var/jabberd/ChatHome/Sites";
my $g_rooms_fsstore_path = "/Library/Server/iChat/Data/fsstore";
my $g_rooms_configs_path = "/private/var/jabberd/Rooms";

#################################  GLOBALS #################################
my $g_purge = "0";		# Default is don't purge
my $g_source_root = "/Previous System";
my $g_source_type = "";
my $g_source_version = "";  # This is the version number of the old system passed into us by Server Assistant. [10.5.x, 10.6.x, or 10.7.x]
my $g_target_root = "/";
my $g_language = "en";	# Should be Tier-0 only in iso format [en, fr, de, ja], we default this to English, en.
my $g_status = 0;		# 0 = success, > 0 on failure
my $g_old_system_plist_path;
my $g_old_server_plist_path;

################################### Flags ###################################
my $DEBUG = 0;
my $FUNC_LOG = 0;

############################## Version Consts  ##############################
my $SYS_VERS = "0";   #10.5.x
my $SYS_MAJOR = "0";  #10
my $SYS_MINOR = "0";  # 5
my $SYS_UPDATE = "-"; #11
my $SRV_VERS = "0";   #10.5.x
my $SRV_MAJOR = "0";  #10
my $SRV_MINOR = "0";  # 5
my $SRV_UPDATE = "-"; #11
my $MINVER = "10.5";  # => 10.5
my $MAXVER = "10.7";  # <  10.7

if ( (defined($ENV{DEBUG})) && ($ENV{DEBUG} eq 1) ) {$DEBUG = '1';}
if ( (defined($ENV{FUNC_LOG})) && ($ENV{FUNC_LOG} eq 1) ) {$FUNC_LOG = '1';}

################################################################################
##############################   MAIN   ########################################
################################################################################

my $mu = new MigrationUtilities;
my @items = $mu->ParseOptions(@ARGV);

if (${DEBUG}) 
	{ $mu->dumpAssociativeArray(@items); }

&validate_options_and_dispatch(@items);
exit($g_status);


################################################################################
##############################   Functions   ###################################
################################################################################

################################################################################
#
sub migrate_upgrade() {
	if ($FUNC_LOG) { print("migrate_upgrade : S\n"); }
	&log_message("migrate_upgrade := S");

	## Need to fix up the paths we care about with the --sourceRoot we received
	$g_old_system_plist_path =  $g_source_root . $g_system_plist_path;
	$g_old_server_plist_path =  $g_source_root . $g_server_plist_path;

	if ($DEBUG) {
		print($g_old_system_plist_path . "\n");
		print($g_old_server_plist_path . "\n");
	}

	# Get old server version parts
	if (${DEBUG}) {printf("sourceVersion := %s\n", "${g_source_version}");}
	($SRV_MAJOR, $SRV_MINOR, $SRV_UPDATE)=$mu->serverVersionParts(${g_source_version});

	# Locate the settings for the service and disable/enable it as needed.
	&restore_and_set_state;

	if ($FUNC_LOG) { print("migrate_upgrade : E\n"); }
	&log_message("migrate_upgrade := E");
}
 
################################################################################
sub get_service_state
{
	if ($FUNC_LOG) {printf("get_service_state := S\n");}
	&log_message("get_service_state := S");
	
	my $src_root = shift;
	my $state;
	if (! defined($src_root)) {
		$src_root = "";
	}
	if (-e "$src_root$g_launchd_overrides_path") {
		$state = qx(${PLISTBUDDY} -c "Print :org.jabber.jabberd:Disabled" "${src_root}${g_launchd_overrides_path}");
		chomp($state);
		if ($state eq "" || $state =~ /Does Not Exist/) {
			# missing entry, treat as disabled
			$state = "true";
		}
	} else {
		$state = qx(${PLISTBUDDY} -c "Print :Disabled" "${src_root}${g_launchd_config_path_jabberd}");
		chomp($state);
		if ($state ne "true") {
			$state = "false";
		}
	}

	if ($FUNC_LOG) {printf("get_service_state := E\n");}
	&log_message("get_service_state := E");
	if ($DEBUG) { &log_message("DEBUG get_service_state returning $state"); }

	return $state;
}

################################################################################
sub restore_and_set_state()
{	
	if ($FUNC_LOG) {printf("restore_and_set_state := S\n");}
	&log_message("restore_and_set_state := S");

	if (! -e $PLISTBUDDY) {
		print "ERROR: \"$PLISTBUDDY\" does not exist.\n";
		exit(1);
	}

	my $ichat_disabled_orig = &get_service_state($g_source_root);
	&log_message("restore_and_set_state: source volume has Disabled = ${ichat_disabled_orig}");
	
	# Make sure that the service at destination has been initialized
	unless (&ensure_ichat_initialized) {
		&log_message("Cannot initialize service, aborting");
		exit(1);
	}

	# Get current state, disable the service if not already disabled
	my $ichat_disabled = &get_service_state;

	if ($ichat_disabled ne "true") {
		$ichat_disabled = "false";
		&start_stop_ichat("stop");  # disable the service
	}

	my $ret;

	if(${SRV_MINOR} eq "6" || ${SRV_MINOR} eq "7") {
		#10.6,10.7 -> 10.7 migration
		# Configs
		if ($DEBUG) {
			$ret = qx (${g_jabber_config_migrator} -d -c "${g_source_root}${g_bundle_config_path}" -s "${g_source_version}");
		} else {
			$ret = qx (${g_jabber_config_migrator} -c "${g_source_root}${g_bundle_config_path}" -s "${g_source_version}");
		}
		if ($? != 0) {
			&log_message("Warning, ${g_jabber_config_migrator} returned error status: $?: $ret");
		}
		# Data
		# If a custom data location is specified in the configuration, look there
		do {{  # not a loop
			my $src_database_location = qx(${PLISTBUDDY} -c "Print :jabberdDatabasePath" "${g_source_root}${g_bundle_config_path}");
			chomp($src_database_location);
			if ($src_database_location =~ /Does Not Exist/) {
				if (${SRV_MINOR} eq "6") {
					$src_database_location = $g_sqlite_db_path;
				} else {
					$src_database_location = $g_sqlite_db_path_pre_10_7;
				}
			} elsif ($src_database_location !~ /^\/Volumes\//) {
				# The path may or may not be on the source root volume
				$src_database_location = "${g_source_root}${src_database_location}";
			}	
			# Make sure source and target are different files
			my $inode_source = (stat($src_database_location))[1];
			my $inode_dst = (stat("${g_target_root}${g_sqlite_db_path}"))[1];
			if (($inode_source != $inode_dst) || ($g_target_root ne $g_source_root)) {
				my $mask = umask;
				umask(027);
				$ret = qx ( $MV -f "${g_target_root}${g_sqlite_db_path}" "${g_target_root}${g_sqlite_db_path}.bak");
				if ($? != 0) {
					&log_message("Warning, backup of original database failed $?: $ret");
				}
				$ret = qx ($CP -v "${src_database_location}" "${g_target_root}${g_sqlite_db_path}");
				if ($? != 0) {
					&log_message("Error, cannot create new database $?: $ret");
				}
				$ret = chown($g_jabber_uid, $g_jabber_gid, "${g_target_root}${g_sqlite_db_path}");
				unless ($ret) {
					&log_message("Error, chown failed with status $ret: $!");
				}
				$ret = chmod(0640, "${g_target_root}${g_sqlite_db_path}");
				unless ($ret) {
					&log_message("Error, chmod failed with status $ret: $!");
				}
				umask($mask);
			}
			# For upgrades from 10.6, add new tables for jabberd 2.2.x compatibility
			if (${SRV_MINOR} eq "6") {
				$ret = open(SQLITE, "|$SQLITE3 \"${g_target_root}${g_sqlite_db_path}\"");
				unless ($ret) {
					&log_message("Error, could not open database file \"${g_target_root}${g_sqlite_db_path}\" using $SQLITE3 : $!");
					last;
				}
				# lifted from jabberd 2.2.11, tools/db-setup.sqlite, diff from jabberd 2.1.24.
				print SQLITE <<"EOF";
CREATE TABLE "published-roster" (
    "collection-owner" TEXT NOT NULL,
    "object-sequence" INTEGER PRIMARY KEY,
    "jid" TEXT NOT NULL,
    "group" TEXT,
    "name" TEXT,
    "to" BOOLEAN NOT NULL,
    "from" BOOLEAN NOT NULL,
    "ask" INTEGER NOT NULL );
CREATE INDEX i_pubrosteri_owner ON "published-roster"("collection-owner");
CREATE TABLE "published-roster-groups" (
    "collection-owner" TEXT NOT NULL,
    "object-sequence" INTEGER PRIMARY KEY,
    "groupname" TEXT NOT NULL );
CREATE INDEX i_pubrosterg_owner ON "published-roster-groups"("collection-owner");
EOF
				close(SQLITE) || &log_message("Error, $SQLITE3 returned an error.  Adding new tables to jabberd database possibly failed.");
			}
		}} while (0);  # not a loop

	} elsif(${SRV_MINOR} eq "5") {
		#10.5 -> 10.7 migration
		# Configs
		if ($DEBUG) {
			$ret = qx (${g_jabber_config_migrator} -d -c "${g_source_root}${g_bundle_config_path}" -s "${g_source_version}");
		} else {
			$ret = qx (${g_jabber_config_migrator} -c "${g_source_root}${g_bundle_config_path}" -s "${g_source_version}");
		}
		if ($? != 0) {
			&log_message("Warning, ${g_jabber_config_migrator} returned error status: $?: $ret");
		}
		# Data
		if ($DEBUG) {
			$ret = qx (${g_jabber_data_migrator_pre_10_7} -D -s "${g_source_root}${g_sqlite_db_path_pre_10_7}" -d "${g_target_root}${g_sqlite_db_path}");
		} else {
			$ret = qx (${g_jabber_data_migrator_pre_10_7} -s "${g_source_root}${g_sqlite_db_path_pre_10_7}" -d "${g_target_root}${g_sqlite_db_path}");
		}
		if ($? != 0) {
			&log_message("Warning, ${g_jabber_data_migrator_pre_10_7} returned error status: $?: $ret");
		}
	}


	# Handle mu-conference -> Rooms migration (persistent room configuration files)
	if (${SRV_MINOR} eq "5" || ${SRV_MINOR} eq "6") {
		&log_message("Migrating mu-conference persistent room configurations");
		my @domains;
		$ret = qx { $PLISTBUDDY -x -c "Print :hosts:" "${g_source_root}${g_bundle_config_path}" };
		chomp($ret);
		my @lines = split("\n", $ret);
		foreach my $line (@lines) {
			if ($line =~ /^(?:[\s]*)<string>(.+)<\/string>$/) {
				push(@domains, $1);
			}
		}
		foreach my $domain (@domains) {
			&log_message("Migrating mu-conference data for domain: ${domain}");
			qx { $MUC_TO_ROOMS_DATA_MIGRATOR -s "${g_source_root}/private/var/spool/conference.${domain}" -d "${g_target_root}${g_rooms_configs_path}/rooms.${domain}" };
		}
	} elsif (${SRV_MINOR} eq "7") {
		do {{  # not a loop
			# Rooms -> Rooms

			# First, chat room configuration files
			&log_message("Migrating Rooms data");
			$ret = opendir(SRC_CONFIG_DIR, "${g_source_root}${g_rooms_configs_path}");
			unless ($ret) {
				&log_message("Could not open directory: ${g_source_root}${g_rooms_configs_path}: $!");
				last;
			}
			my @dirs = readdir(SRC_CONFIG_DIR);
			closedir(SRC_CONFIG_DIR);
			foreach my $dir (@dirs) {
				if (! -d "${g_source_root}${g_rooms_configs_path}/${dir}" || $dir eq "." || $dir eq "..") { next; }
				$ret = qx { $CP -v -p -R "${g_source_root}${g_rooms_configs_path}/${dir}" "${g_target_root}${g_rooms_configs_path}" };
				if ($? != 0) {
					&log_message("Error, cp failed with status $ret: ($?)");
					next;
				}
			}

			# Now, migrate any data related to file sharing
			# uploaded and shared files:
			$ret = opendir(SRC_DATA_DIR, "${g_source_root}${g_rooms_fsstore_path}");
			unless ($ret) {
				&log_message("Could not open directory: ${g_source_root}${g_rooms_fsstore_path}: $!");
				last;
			}
			@dirs = readdir(SRC_DATA_DIR);
			closedir(SRC_DATA_DIR);
			foreach my $dir (@dirs) {
				if (! -d "${g_source_root}${g_rooms_fsstore_path}/${dir}" || $dir eq "." || $dir eq "..") { next; }
				$ret = qx { $CP -v -p -R "${g_source_root}${g_rooms_fsstore_path}/${dir}" "${g_target_root}${g_rooms_fsstore_path}" };
				if ($? != 0) {
					&log_message("Error, cp failed with status $ret: ($?)");
					next;
				}
			}
			
			# Create directories and symlinks
			$ret = opendir(DOMAINS, "${g_source_root}${g_rooms_fsstore_path}");
			unless ($ret) {
				&log_message("Could not open directory: ${g_source_root}${g_rooms_fsstore_path}: $!");
				last;
			}
			my @domains = readdir(DOMAINS);
			closedir(DOMAINS);
			foreach my $domain (@domains) {
				if (! -d $domain || $domain eq "." || $domain eq "..") { next; }

				$ret = opendir(ROOMS, "${g_source_root}${g_rooms_fsstore_path}/${domain}");
				unless ($ret) {
					&log_message("Could not open directory: ${g_source_root}${g_rooms_fsstore_path}/${domain} : $!");
					next;
				}
				my @rooms = readdir(ROOMS);
				closedir(ROOMS);
				foreach my $room (@rooms) {
				if (! -d $room || $room eq "." || $room eq "..") { next; }
					my $guid = qx { $PLISTBUDDY -c "Print ${room}\@${domain}:uniqueID:" "${g_source_root}${g_rooms_fsstore_path}/${domain}/${room}/room_fsaccess.plist" };
					chomp($guid);
					if ($guid =~ /Does Not Exist/) {
						&log_message("$PLISTBUDDY failed ($?): ${guid}");
						next;
					} else {
						$ret = mkdir("${g_target_root}${g_rooms_home_sites_path}/${guid}", 0750);
						unless ($ret) {
							&log_message("Could not create directory ${g_target_root}${g_rooms_home_sites_path}/${guid} : $!");
							next;
						}
						$ret = chown($g_jabber_uid, $g_jabber_gid, "${g_target_root}${g_rooms_home_sites_path}/${guid}");
						unless ($ret) {
							&log_message("Error, chown failed with status $ret: $!");
						}
					}
				}
			}
		}} while (0);  # not a loop
	}


	# Backup old message logs
	do {{  # not a loop
		&log_message("Migrating message archives to new system...");
		my $source_archive_dir = qx(${PLISTBUDDY} -c "Print :savedChatsLocation" "${g_source_root}${g_bundle_config_path}");
		chomp($source_archive_dir);
		if (! -e "${g_source_root}${source_archive_dir}") {
			&log_message("Could not locate source directory for message archive migration");
			last;
		}

		my $target_archive_dir = qx(${PLISTBUDDY} -c "Print :savedChatsLocation" "${g_target_root}${g_bundle_config_path}");
		chomp($target_archive_dir);
		if (! -e $target_archive_dir) {
			&log_message("Could not locate target directory for message archive migration");
			last;
		}
		my $archive_backup_dir = "${target_archive_dir}_${g_source_version}";
		my $i = 0;
		while (-e $archive_backup_dir && ($i < 100)) {
			$archive_backup_dir = "${target_archive_dir}_${g_source_version}_${i}";
			$i++;
		}
		if (-e $archive_backup_dir) {
			&log_message("Error: could not migrate message archives: can't create destination directory");
			last;
		}

		$ret = mkdir($archive_backup_dir, 0750);
		unless ($ret) {
			&log_message("Error, mkdir failed with status $ret: $!");
			last;
		}

		$ret = chown($g_jabber_uid, $g_admin_gid, $archive_backup_dir);
		unless ($ret) {
			&log_message("Error, chown failed with status $ret: $!");
			last;
		}

		$ret = opendir(SRC_MSG_DIR, "${g_source_root}${source_archive_dir}");
		unless ($ret) {
			&log_message("Could not open directory: ${g_source_root}${source_archive_dir} : $!");
			last;
		}
		my @files = readdir(SRC_MSG_DIR);
		closedir(SRC_MSG_DIR);
		foreach my $file (@files) {
			if (-d $file) { next; }
			$ret = qx { $CP -v -p "${g_source_root}${source_archive_dir}/${file}" "${archive_backup_dir}" };
			if ($? != 0) {
				&log_message("Error, cp failed with status $ret: ($?)");
			}
		}
	}} while (0);  # not a loop

	# Start/Stop and Load/Unload, using source volume's service state
	$ichat_disabled = &get_service_state;

	if (($ichat_disabled_orig eq "false") && ($ichat_disabled eq "true")) {
		&log_message("restore_and_set_state: Starting iChat Server service");
		&start_stop_ichat("start");
	}

	if ($FUNC_LOG) {printf("restore_and_set_state := E\n");}
	&log_message("restore_and_set_state := E");
}			

################################################################################
##
sub start_stop_ichat()
{
	my $command = shift;
	my $ichat_disabled = &get_service_state;

	if ($FUNC_LOG) {printf("start_stop_ichat := S\n");}
	&log_message("start_stop_ichat := S");

	if (($command eq "start") &&
			($ichat_disabled eq "true")) {
		&log_message("Starting iChat Server service");
		qx(${SERVERADMIN} start ${g_service_name});
		if ($? != 0) { &log_message("${SERVERADMIN} failed with status error status: $?\n"); }
		if ($DEBUG) { printf("%s\n", qq(${SERVERADMIN} start ${g_service_name})); }
	} elsif (($command eq "stop") &&
			($ichat_disabled eq "false")) { 
		&log_message("Stopping iChat Server service");
		qx(${SERVERADMIN} stop ${g_service_name});
		if ($? != 0) { &log_message("${SERVERADMIN} failed with status error status: $?\n"); }
		if ($DEBUG) { printf("%s\n", qq(${SERVERADMIN} stop ${g_service_name}));  }
	} else {
		if ($DEBUG) { &log_message("start_stop_ichat: nop, command = ${command}, ichat_disabled = ${ichat_disabled}"); }
	}

	if ($FUNC_LOG) {printf("start_stop_ichat := E\n");}
	&log_message("start_stop_ichat := E");	
}

################################################################################
## If the service hasn't been initialized yet, do it now.		
sub ensure_ichat_initialized()
{
	if ($FUNC_LOG) {printf("ensure_ichat_initialized := S\n");}
	&log_message("ensure_ichat_initialized := S");

	if (-e $g_bundle_config_path) {
		my $ichat_initialized = qx(${PLISTBUDDY} -c "Print :initialized" "${g_bundle_config_path}");
		chomp($ichat_initialized);
		if ($ichat_initialized eq "true") {
			&log_message("Already initialized");
			return 1;
		}
	}

	&log_message("Issuing initialSetup command for iChat Server initialization");
	#  Otherwise we need to do the initialization ourself
	my $ret = qx(${SERVERADMIN} status ${g_service_name});
	&log_message("getState returned: $ret");

	if (-e $g_bundle_config_path) {
		my $ichat_initialized = qx(${PLISTBUDDY} -c "Print :initialized" "${g_bundle_config_path}");
		chomp($ichat_initialized);
		if ($ichat_initialized ne "true") {
			&log_message("Error: Cannot initialize service");
			return 0;
		}
	}

	if ($FUNC_LOG) {printf("ensure_ichat_initialized := E\n");}
	&log_message("ensure_ichat_initialized := E");
	return 1;
}
  
################################################################################
## Service-specific log	
sub log_message()
{
	if (! -e $g_migration_log_dir) {
		my $ret = mkdir("${g_target_root}${g_migration_log_dir}", 0755);
		unless ($ret) {
			print "Cannot create directory for migration log\n";
			return;
		}
	}
	if (! open(LOGFILE, ">>$g_log_path")) {
		print "$0: cannot open $g_log_path: $!";
		return;
	}
	my $time = localtime();
	print LOGFILE "$time: ".basename($0).": @_\n";
	print "@_\n" if $DEBUG;
	close(LOGFILE);
}

################################################################################
sub validate_options_and_dispatch()
{
	my %big_list = @_;
	my $valid;
	my $nothing = 0;

	#Set the globals with the options passed in.
	if ($big_list{"--purge"}) {
		$g_purge = $big_list{"--purge"};
	}
	if ($big_list{"--sourceRoot"}) {
		$g_source_root = $big_list{"--sourceRoot"};
	}
	if ($big_list{"--sourceType"}) {
		$g_source_type=$big_list{"--sourceType"};
	}
	if ($big_list{"--sourceVersion"}) {
		$g_source_version=$big_list{"--sourceVersion"};
	}
	if ($big_list{"--targetRoot"}) {
		$g_target_root=$big_list{"--targetRoot"};
	}
	if ($big_list{"--language"}) {
		$g_language=$big_list{"--language"};
	}

	qx(/bin/echo purge: $g_purge >> $g_shared_log_path);
	qx(/bin/echo sourceRoot: $g_source_root >> $g_shared_log_path);
	qx(/bin/echo sourceType: $g_source_type >> $g_shared_log_path);
	qx(/bin/echo sourceVersion: $g_source_version >> $g_shared_log_path);
	qx(/bin/echo targetRoot: $g_target_root >> $g_shared_log_path);
	qx(/bin/echo language: $g_language >> $g_shared_log_path);

	SWITCH: {
		if( ($mu->pathExists("${g_source_root}")) && ($mu->pathExists("${g_target_root}")) ) {
			if ($mu->isValidLanguage($g_language)) {
				if ($mu->isValidVersion($g_source_version)) {
					$valid = 1;
					&migrate_upgrade();
				} else {
					print("Did not supply a valid version for the --sourceVersion parameter, needs to be >= 10.5.0 and < 10.7.0\n");
					&usage(); exit(1);
				}
			} else {
				print("Did not supply a valid language for the --language parameter, needs to be one of [en|fr|de|ja]\n");
				&usage(); exit(1);
			}
		} else {
			print("Source and|or destination for upgrade/migration does not exist.\n");
			&usage(); exit(1);
		} last SWITCH;
		$nothing = 1;
	}
	if($nothing == 1)
		{print("Legal options were not supplied!\n"); &usage();}
}

################################################################################
# Show proper usage
sub usage()
{
	print("--purge <0 | 1>   \"1\" means remove any files from the old system after you've migrated them, \"0\" means leave them alone." . "\n");
	print("--sourceRoot <path> The path to the root of the system to migrate" . "\n");
	print("--sourceType <System | TimeMachine> Gives the type of the migration source, whether it's a runnable system or a " . "\n");
	print("                  Time Machine backup." . "\n");
	print("--sourceVersion <ver> The version number of the old system (like 10.5.x or 10.6). Since we support migration from 10.5, " . "\n");
	print("                  10.6, and other 10.7 installs, it's useful to know this information, and it would be easier for me to figure " . "\n");
	print("                  it out once and pass it on to each script than to have each script have to figure it out itself." . "\n");
	print("--targetRoot <path> The path to the root of the new system. Pretty much always \"\/\"" . "\n");
	print("--language <lang> A language identifier, such as \"en.\" Long running scripts should return a description of what they're doing" . "\n");
	print("                  (\"Migrating Open Directory users\"), and possibly provide status update messages along the way. These messages" . "\n");
	print("                  need to be localized (which is not necessarily the server running the migration script)." . "\n");
	print("                  This argument will identify the Server Assistant language. As an alternative to doing localization yourselves" . "\n");
	print("                  send them in English, but in case the script will do this it will need this identifier." . "\n");
	print(" " . "\n");
}
