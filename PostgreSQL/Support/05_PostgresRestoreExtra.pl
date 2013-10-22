#!/usr/bin/perl
#
# 05_PostgresRestoreExtra.pl
#
# Author:: Apple Inc.
# Documentation:: Apple Inc.
# Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
#
# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
# License:: All rights reserved.
#
# RestoreExtra script for PostgreSQL
# Unpack any possible backup data from previous OS backups for consumption by PostgreSQL MigrationExtra
# For postgres, the most recent SQL dump should be found beneath /Library/Server/Previous,
#   not beneath .ServerBackups, so restore it for the MigrationExtra to use.
# perltidy options: -pbp -l=100

use strict;
use warnings;
use File::Basename qw(basename);
use File::Path qw(rmtree);
use Errno qw(EAGAIN);

my $GZCAT       = '/usr/bin/gzcat';
my $INITDB      = '/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0/initdb';
my $MKTEMP_PATH = '/usr/bin/mktemp';
my $POSTGRES    = '/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0/postgres';
my $PSQL        = '/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0/psql';
my $SUDO        = '/usr/bin/sudo';
my $g_log_dir   = '/Library/Logs/Migration';
my $g_log_path  = $g_log_dir . '/PostgreSQLRestoreExtra.log';
my $g_src_path  = '/Library/Server/Previous';
my $g_target_path      = '/Library/Server/Previous';
my $g_postgres_log_dir = '/Library/Logs/PostgreSQL';
my $g_postgres_launchd_plist
    = $g_src_path . '/System/Library/LaunchDaemons/org.postgresql.postgres.plist';
my $g_postgres_uid   = 216;
my $g_postgres_gid   = 216;
my $g_source_version = q{};

parse_options();

restore_for_migration_extra();

exit 0;

################################################################################
sub restore_for_migration_extra {

    # Create the log dir if it doesn't exist.
    my $ret = 0;
    if ( !-e $g_postgres_log_dir ) {
        if ( !mkdir $g_postgres_log_dir, 0755 ) {
            bail("Error, mkdir failed: $!");
        }
        if ( !chown $g_postgres_uid, $g_postgres_gid, $g_postgres_log_dir ) {
            bail("Error, chown failed$!");
        }
    }

    # Move aside any existing database in the target location.
    my $db_dir = q{};
    if ( $g_source_version =~ /^10.7/ ) {

# There is an issue where if the data location was changed to an alternate volume, then changed back to the initial volume,
# Server.app sets the data directory to a different location than the default.  So use the newer directory if it exists.
        if ( -e $g_target_path . '/Library/Server/PostgreSQL/Data' ) {
            $db_dir = $g_target_path . '/Library/Server/PostgreSQL/Data';
        }
        else {
            $db_dir = $g_target_path . '/private/var/pgsql';
        }
    }
    elsif ( $g_source_version =~ /^10.6/ ) {
        log_message('PostgreSQL migration from 10.6 is not supported.  Exiting.');
        exit 0;
    }
    else {
        bail("Error: unrecognized value for source version: $g_source_version");
    }
    if ( -e $db_dir ) {
        my $db_dir_backup_path = $db_dir . '_PostgresRestoreExtra_original_' . timestamp();
        if ( !rename $db_dir, $db_dir_backup_path ) {
            bail("Error, rename failed: $!");
        }
    }

    # Determine if the most recent SQL dump is located on an alternate partition.
    my $LAUNCHD_PLIST;
    if ( !open $LAUNCHD_PLIST,
        '<', '/Library/Server/Previous/System/Library/LaunchDaemons/org.postgresql.postgres.plist' )
    {
        bail('Could not locate migrated launchd plist for postgres');
    }
    my @plist_lines = <$LAUNCHD_PLIST>;
    close $LAUNCHD_PLIST;
    my $previous_data_location = q{};
    my $use_next_value         = 0;
    foreach my $plist_line (@plist_lines) {
        if ($use_next_value) {
            if ( $plist_line =~ /<string>(.*)<\/string>/ ) {
                $previous_data_location = $1;
            }
            else {
                bail('Error finding the data location when parsing the migrated launchd plist');
            }
            last;
        }
        if ( $plist_line =~ /<string>-D<\/string>/ ) {
            $use_next_value = 1;
        }
    }
    if ( $previous_data_location eq q{} ) {
        bail('Error determining previous data location');
    }
    my $src_data_gz = q{};
    if ( $previous_data_location =~ /^\/Volumes\// ) {

        # data was on an alternate volume, so our backup file will be on that volume also
        if ( $previous_data_location =~ /(.*)\/Data$/ ) {
            $src_data_gz = $1 . '/Backup/dumpall.psql.gz';
            log_message("Using the following file for database migration: $src_data_gz");
        }
        else {
            bail('Unexpected path found for previous data location');
        }
    }
    else {
        $src_data_gz = $g_src_path . '/Library/Server/PostgreSQL/Backup/dumpall.psql.gz';
    }

    if ( !-e $src_data_gz ) {
        bail(
            "Error: did not find a source database file for database restoration. File expected at $src_data_gz"
        );
    }

    if ( !mkdir $db_dir, 0700 ) {
        bail("Error: mkdir failed: $!");
    }
    if ( !chown $g_postgres_uid, $g_postgres_gid, $db_dir ) {
        bail("Error, chown failed: $!");
    }

    # Init the database then replay the backup SQL for restore
    my $cmd = "$SUDO -u _postgres $INITDB --encoding UTF8 -D \"$db_dir\"";
    if ( run($cmd) != 0 ) {
        bail('Error: necessary command failed.');
    }

    # Make a temp dir for the socket in case some other instance of postgres is running
    my $mask = umask;
    umask 077;
    my $tmp_socket_dir = q{};
    for ( my $i = 0; $i < 5; $i++ ) {
        $tmp_socket_dir
            = `$MKTEMP_PATH -d /tmp/postgres_restoreExtra_socket.XXXXXXXXXXXXXXXXXXXXXXXX`;
        chomp $tmp_socket_dir;
        if ( $tmp_socket_dir =~ /failed/ ) {
            next;
        }
        if ( -e $tmp_socket_dir ) {
            last;
        }
        if ( $i == 4 ) {
            bail("Error: Cannot create temporary file: $tmp_socket_dir");
        }
    }
    umask $mask;
    if ( !chown $g_postgres_uid, $g_postgres_gid, $tmp_socket_dir ) {
        bail("Error, chmod failed: $!");
    }

    # launch postgres in the background using the target database
    $cmd
        = "$SUDO -u _postgres $POSTGRES -D \"$db_dir\" -c listen_addresses= -c log_connections=on -c log_directory=/Library/Logs/PostgreSQL -c log_filename=PostgreSQL_RestoreExtra.log -c log_line_prefix=%t -c log_lock_waits=on -c log_statement=ddl -c logging_collector=on -c unix_socket_directory=$tmp_socket_dir -c unix_socket_group=_postgres -c unix_socket_permissions=0770";
    my $pg_pid;
FORK: {
        if ( $pg_pid = fork ) {
            log_message("Starting postgres with command: $cmd");
        }
        elsif ( defined $pg_pid ) {
            exec $cmd;
        }
        elsif ( $! == EAGAIN ) {
            sleep 5;
            redo FORK;
        }
        else {
            rmtree($tmp_socket_dir);
            bail('Error: fork error');
        }
    }
    for ( my $i = 0; $i <= 5; $i++ ) {
        $ret = system "$PSQL -h $tmp_socket_dir -U _postgres postgres -c \"\\q\" &>/dev/null";
        if ( $ret == 0 ) {
            last;
        }
        elsif ( $i == 5 ) {
            rmtree($tmp_socket_dir);
            bail('Could not connect to postgres for restore.');
        }
        else {
            sleep 1;
        }
    }
    log_message('...replaying database contents (this may take a while)...');
    $cmd = "$GZCAT $src_data_gz | $PSQL -h $tmp_socket_dir -U _postgres postgres";
    my $restore_failed = 0;
    if ( run($cmd) != 0 ) {
        $restore_failed = 1;
    }

    log_message("Terminating postgres with parent process $pg_pid...");
    kill 'TERM', $pg_pid;
    waitpid $pg_pid, 0;
    rmtree($tmp_socket_dir);

    if ($restore_failed) {
        bail('Error: Restore failed.');
    }
    else {
        log_message('...Restore succeeded.');
    }
}

################################################################################
sub parse_options {
    my $argv_size = @ARGV;
    for ( my $i = 0; $i < ( $argv_size - 1 ); $i++ ) {
        if ( $ARGV[$i] eq '--sourceVersion' ) {
            $g_source_version = $ARGV[ $i + 1 ];
            last;
        }
    }
}

################################################################################
sub run {
    my $command = shift;
    log_message("Executing command: $command");
    my $mask = umask;
    umask 077;
    my $data_tmp_dir = q{};
    for ( my $i = 0; $i < 5; $i++ ) {
        $data_tmp_dir = `$MKTEMP_PATH -d /tmp/postgres_restoreExtra.XXXXXXXXXXXXXXXXXXXXXXXX`;
        chomp $data_tmp_dir;
        if ( $data_tmp_dir =~ /failed/ ) {
            next;
        }
        if ( -e $data_tmp_dir ) {
            last;
        }
        if ( $i == 4 ) {
            bail("Error: Cannot create temporary file: $data_tmp_dir");
        }
    }
    umask $mask;

    my $ret = system "$command 1> $data_tmp_dir/cmd.output 2>&1";
    if ( $ret != 0 ) {
        my $msg = "Error executing command. Return code: $ret";
        if ( -e $data_tmp_dir . '/cmd.output' ) {
            $msg .= "\nOutput was: ";
            my $OUTPUT;
            if ( open $OUTPUT, '<', $data_tmp_dir . '/cmd.output' ) {
                my @lines = <$OUTPUT>;
                close $OUTPUT;
                foreach my $line (@lines) {
                    chomp $line;
                    $msg .= "\n" . $line;
                }
            }
        }
        rmtree($data_tmp_dir);
        log_message($msg);
        return 1;
    }
    rmtree($data_tmp_dir);
    return 0;
}

################################################################################
sub timestamp {
    my ( $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst ) = localtime;
    $year += 1900;
    $mon  += 1;
    if ( $mday =~ / \A \d \z /xms ) { $mday = '0' . $mday; }
    if ( $mon  =~ / \A \d \z /xms ) { $mon  = '0' . $mon; }
    if ( $hour =~ / \A \d \z /xms ) { $hour = '0' . $hour; }
    if ( $min  =~ / \A \d \z /xms ) { $min  = '0' . $min; }
    if ( $sec  =~ / \A \d \z /xms ) { $sec  = '0' . $sec; }

    my $ret = "$year-$mon-$mday $hour:$min:$sec";

    return $ret;
}

################################################################################
# Handle the various output modes, log to our file
sub log_message {
    if ( !-e $g_log_dir ) {
        if ( !mkdir $g_log_dir, 0755 ) {
            print "Cannot create directory for log\n";
            return;
        }
    }
    my $LOGFILE;
    if ( !open $LOGFILE, '>>', $g_log_path ) {
        print "$0: cannot open $g_log_path: $!\n";
        return;
    }
    print $LOGFILE timestamp() . q{ } . basename($0) . ": @_\n";
    close $LOGFILE;
}

################################################################################
sub bail {
    log_message(@_);
    log_message('Aborting!');
    print "@_\n";
    exit 1;
}
