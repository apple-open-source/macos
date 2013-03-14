#!/usr/bin/perl -w
# Copyright (c) 2012 Apple Inc. All Rights Reserved.
#
# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
#
# 05_PostgresRestoreExtra.pl
# RestoreExtra script for PostgreSQL
# Unpack any possible backup data from previous OS backups for consumption by PostgreSQL MigrationExtra
# For postgres, the most recent SQL dump should be found beneath /Library/Server/Previous,
#   not beneath .ServerBackups, so restore it for the MigrationExtra to use.

BEGIN {
	if ( -e "/Applications/Server.app/Contents/ServerRoot/System/Library/ServerSetup/MigrationExtras/" ) {
		push @INC,"/Applications/Server.app/Contents/ServerRoot/System/Library/ServerSetup/MigrationExtras/";
	}
	elsif ( -e "/System/Library/ServerSetup/MigrationExtras/" ) {
		push @INC,"/System/Library/ServerSetup/MigrationExtras/";
	}
	else {
		print "Server Migration perl lib can not be found.\n";
	}
}

use strict;
use warnings;
use File::Basename 'basename';
use File::Path 'rmtree';
use MigrationUtilities;

my $GZCAT = "/usr/bin/gzcat";
my $INITDB = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0/initdb";
my $MKDIR = "/bin/mkdir";
my $MKTEMP_PATH = "/usr/bin/mktemp";
my $MV = "/bin/mv";
my $POSTGRES_REAL = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0/postgres_real";
my $PSQL = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0/psql";
my $SUDO = "/usr/bin/sudo";
my $g_log_dir = "/Library/Logs/Migration";
my $g_log_path = $g_log_dir."/PostgreSQLRestoreExtra.log";
my $g_src_path = "/Library/Server/Previous";
my $g_target_path = "/Library/Server/Previous";
my $g_postgres_log_dir = "/Library/Logs/PostgreSQL";
my $g_postgres_uid = 216;
my $g_postgres_gid = 216;
my $g_source_version = "";

my $g_mu = new MigrationUtilities;
my @g_items = $g_mu->ParseOptions(@ARGV);

&validate_options_and_dispatch(@g_items);

&restore_for_migration_extra;

exit(0);

################################################################################
sub restore_for_migration_extra() {
	# Create the log dir if it doesn't exist.
	my $ret = 0;
	if (! -e $g_postgres_log_dir) {
		$ret = mkdir($g_postgres_log_dir);
		unless ($ret) {	&bail("Error, mkdir failed with status $ret: $!"); }
		$ret = chown($g_postgres_uid, $g_postgres_gid, $g_postgres_log_dir);
		unless ($ret) {	&bail("Error, chown failed with status $ret: $!"); }
		$ret = chmod(0755, $g_postgres_log_dir);
		unless ($ret) {	&bail("Error, chmod failed with status $ret: $!"); }
	}

	# Move aside any existing database in the target location.
	my $db_dir = "";
	if ($g_source_version =~ /^10.7/) {
		$db_dir = $g_target_path."/private/var/pgsql";
	} elsif ($g_source_version =~ /^10.8/) {
		$db_dir = $g_target_path."/Library/Server/PostgreSQL/Data";
	} elsif ($g_source_version =~ /^10.6/) {
		&log_message("PostgreSQL migration from 10.6 is not supported.  Exiting.");
		exit(0);
	} else {
		&bail("Error: unrecognized value for source version: ${g_source_version}");
	}
	if (-e $db_dir) {
		my $db_dir_backup_path = $db_dir."_PostgresRestoreExtra_original_".&timestamp;
		if (&run("$MV \"$db_dir\" \"$db_dir_backup_path\"")) {
			&bail("Error, necessary command failed.");
		}
	}

	my $src_data_gz = $g_src_path."/Library/Server/PostgreSQL/Backup/dumpall.psql.gz";
	if (! -e $src_data_gz) {
		&bail("Error: did not find a source database file for database restoration. File expected at ${src_data_gz}");
	}

	if (&run("$MKDIR -p ${db_dir}")) { &bail("Error: necessary command failed."); }
	$ret = chown($g_postgres_uid, $g_postgres_gid, $db_dir);
	unless ($ret) {	&bail("Error, chown failed with status $ret: $!"); }
	$ret = chmod(0750, $db_dir);
	unless ($ret) {	&bail("Error, chmod failed with status $ret: $!"); }

	# Init the database then replay the backup SQL for restore
	my $cmd = "$SUDO -u _postgres $INITDB --encoding UTF8 -D ${db_dir}";
	if (&run($cmd)) { &bail("Error: necessary command failed."); }

	# Make a temp dir for the socket in case some other instance of postgres is running
	my $mask = umask;
	umask(077);
	my $tmp_socket_dir = "";
	for (my $i = 0; $i < 5; $i++) {
		$tmp_socket_dir = `$MKTEMP_PATH -d /tmp/postgres_restoreExtra_socket.XXXXXXXXXXXXXXXXXXXXXXXX`;
		chomp($tmp_socket_dir);
		if ($tmp_socket_dir =~ /failed/) {
			next;
		}
		if (-e $tmp_socket_dir) {
			last;
		}
		if ($i == 4) {
			&bail("Error: Cannot create temporary file:\n${tmp_socket_dir}");
		}
	}
	umask($mask);
	$ret = chown($g_postgres_uid, $g_postgres_gid, $tmp_socket_dir);
	unless ($ret) {	&bail("Error, chmod failed with status $ret: $!"); }

	# launch postgres in the background using the target database
	$cmd = "$SUDO -u _postgres $POSTGRES_REAL -D ${db_dir} -c listen_addresses= -c log_connections=on -c log_directory=/Library/Logs/PostgreSQL -c log_filename=PostgreSQL_RestoreExtra.log -c log_line_prefix=%t  -c log_lock_waits=on -c log_statement=ddl -c logging_collector=on -c unix_socket_directory=${tmp_socket_dir} -c unix_socket_group=_postgres -c unix_socket_permissions=0770";
	my $pg_pid;
	FORK: {
		if ($pg_pid  = fork) {
			&log_message("Starting postgres with command: ${cmd}");
		} elsif (defined $pg_pid) {
			exec($cmd) or &bail("Error starting postgres: $!");
		} else {
			rmtree($tmp_socket_dir);
			&bail("Error: fork error")
		}
	}
	for (my $i = 0; $i <= 5; $i++) {
		$ret = system("$PSQL -h ${tmp_socket_dir} -U _postgres postgres -c \"\\q\" &>/dev/null");
		&log_message("DEBUG: failed to connect to postgres");
		if ($ret == 0) { last; }
		elsif ($i == 5) {
			rmtree($tmp_socket_dir);
			&bail("Could not connect to postgres for restore.");
		} else {
			sleep(1);
		}
	}
	&log_message("...replaying database contents (this may take a while)...");
	$cmd = "$GZCAT ${src_data_gz} | $PSQL -h ${tmp_socket_dir} -U _postgres postgres";
	my $restore_failed = 0;
	if (&run($cmd)) { $restore_failed = 1; }

	&log_message("Terminating postgres with parent process ${pg_pid}...");
	kill('TERM', $pg_pid);
	waitpid($pg_pid, 0);
	rmtree($tmp_socket_dir);

	if ($restore_failed) {
		&bail("Error: Restore failed.");
	} else {
		&log_message("...Restore succeeded.");
	}
}

################################################################################
sub validate_options_and_dispatch()
{
	my %big_list = @_;
	
	#Set the globals with the options passed in.
	#	if ($big_list{"--purge"}) {
	#	$g_purge = $big_list{"--purge"};
	#}
	#if ($big_list{"--sourceRoot"}) {
	#	$g_source_root = $big_list{"--sourceRoot"};
	#}
	#if ($big_list{"--sourceType"}) {
	#	$g_source_type=$big_list{"--sourceType"};
	#}
	if ($big_list{"--sourceVersion"}) {
		$g_source_version=$big_list{"--sourceVersion"};
	} else {
		&bail("Required argument for --sourceVersion was not provided or is invalid");
	}
	#if ($big_list{"--targetRoot"}) {
	#	$g_target_root=$big_list{"--targetRoot"};
	#}
	#if ($big_list{"--language"}) {
	#	$g_language=$big_list{"--language"};
	#}
	
	#qx(/bin/echo purge: $g_purge >> $g_shared_log_path);
	#qx(/bin/echo sourceRoot: $g_source_root >> $g_shared_log_path);
	#qx(/bin/echo sourceType: $g_source_type >> $g_shared_log_path);
	#qx(/bin/echo sourceVersion: $g_source_version >> $g_shared_log_path);
	#qx(/bin/echo targetRoot: $g_target_root >> $g_shared_log_path);
	#qx(/bin/echo language: $g_language >> $g_shared_log_path);

}

################################################################################
sub run() {
	my $command = shift;
	&log_message("Executing command: ${command}");
	my $mask = umask;
	umask(077);
	my $data_tmp_dir = "";
	for (my $i = 0; $i < 5; $i++) {
		$data_tmp_dir = `$MKTEMP_PATH -d /tmp/postgres_restoreExtra.XXXXXXXXXXXXXXXXXXXXXXXX`;
		chomp($data_tmp_dir);
		if ($data_tmp_dir =~ /failed/) {
			next;
		}
		if (-e $data_tmp_dir) {
			last;
		}
		if ($i == 4) {
			&bail("Error: Cannot create temporary file:\n${data_tmp_dir}");
		}
	}
	umask($mask);
	
	my $ret = system("${command} 1> ${data_tmp_dir}/cmd.output 2>&1");
	if ($ret != 0) {
		my $msg = "Error executing command. Return code: ${ret}";
		if (-e "${data_tmp_dir}/cmd.output}") {
			$msg .= "\nOutput was:";
			open(OUTPUT, "<${data_tmp_dir}/cmd.output");
			my @lines = <OUTPUT>;
			close(OUTPUT);
			foreach my $line (@lines) {
				chomp($line);
				$msg .= "\n${line}";
			}
		}
		rmtree($data_tmp_dir);
		&log_message($msg);
		return 1;
	}
	rmtree($data_tmp_dir);
	return 0;
}
################################################################################
sub timestamp()
{
	my ( $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst ) =
	localtime(time);
	$year += 1900;
	$mon  += 1;
	if ( $hour =~ /^\d$/ ) { $hour = "0" . $hour; }
	if ( $min  =~ /^\d$/ ) { $min  = "0" . $min; }
	if ( $sec  =~ /^\d$/ ) { $sec  = "0" . $sec; }
	
	my $ret = $year."-".$mon."-".$mday."-${hour}:${min}:${sec}";
	return $ret;
}

################################################################################
# Handle the various output modes, log to our file
sub log_message {
	if (! -e $g_log_dir) {
		my $ret = mkdir("${g_log_dir}", 0755);
		unless ($ret) {
			print "Cannot create directory for log\n";
			return;
		}
	}
	if (! open(LOGFILE, ">>${g_log_path}")) {
		print "$0: cannot open ${g_log_path}: $!";
		return;
	}
	print LOGFILE &timestamp.": ".basename($0).": @_\n";
	close(LOGFILE);
}

################################################################################
sub bail
{
    &log_message(@_);
    &log_message("Aborting!");
    print "@_\n";
    exit(1);
}
