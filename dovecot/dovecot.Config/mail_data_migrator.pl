#!/usr/bin/perl
#
# Copyright (c) 2010-2011 Apple Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without  
# modification, are permitted provided that the following conditions  
# are met:
# 
# 1.  Redistributions of source code must retain the above copyright  
# notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above  
# copyright notice, this list of conditions and the following  
# disclaimer in the documentation and/or other materials provided  
# with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
# contributors may be used to endorse or promote products derived  
# from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
# SUCH DAMAGE.
#

use strict;

$| = 1;

## Mail Data Migrator

############################# System  Constants #############################
my $CAT = "/bin/cat";
my $CP = "/bin/cp";
my $MV = "/bin/mv";
my $RM = "/bin/rm";
my $DSCL = "/usr/bin/dscl";
my $DU = "/usr/bin/du";
my $ECHO = "/bin/echo";
my $GREP = "/usr/bin/grep";
my $CHOWN = "/usr/sbin/chown";
my $LAUNCHCTL = "/bin/launchctl";
my $POSTCONF = "/usr/sbin/postconf";
my $MKDIR = "/bin/mkdir";
my $SERVER_ADMIN = "/usr/sbin/serveradmin";
my $PLIST_BUDDY = "/usr/libexec/PlistBuddy";
my $CVT_MAIL_DATA = "/usr/bin/cvt_mail_data";
my $TAR = "/usr/bin/tar";

################################## Consts ###################################
my $MIGRATION_LOG= "/Library/Logs/MailMigration.log";

################################## Globals ##################################
my @g_clean_partitions	= ();
my $g_migration_plist	= "/var/db/.mailmigration.plist";
my $g_imapd_conf		= "/etc/imapd.conf";
my $g_source_root		= "";
my $g_target_root		= "";
my $g_source_version	= "";
my $g_sl_src			= 0;	# Set to 1 if source is Snow Leopard Server
my $g_db_path			= "/Previous System/private/var/imap";
my $g_purge				= 0;

############################## Version Consts  ##############################
my $SYS_VERS	= "0";	#10.4.11
my $SYS_MAJOR	= "0";	#10
my $SYS_MINOR	= "0";	# 4
my $SYS_UPDATE	= "-";	#11
my $SRV_VERS	= "0";	#10.4.11
my $SRV_MAJOR	= "0";	#10
my $SRV_MINOR	= "0";	# 4
my $SRV_UPDATE	= "-";	#11
my $MIN_VER		= "10.4"; # => 10.4
my $MAX_VER		= "10.8"; # <  10.7
my $TARGET_VER	= "10.7";

################################### Flags ###################################
my $DEBUG		= 0;
my $FUNC_LOG	= 0;

#############################################################################
# main
#############################################################################

use Foundation;
use File::Copy;
use File::Basename;

if ($ENV{DEBUG} eq 1) {
	$DEBUG = '1'; }

if ($ENV{FUNC_LOG} eq 1) {
	$FUNC_LOG = '1'; }

open (LOG_FILE, ">> ${MIGRATION_LOG}" ) or die("$MIGRATION_LOG: $!\n");

do_data_migration();

# cleanup mail migration data plist
qx( ${RM} -f "${g_migration_plist}" 2>&1 >> ${MIGRATION_LOG} );

exit();

################################################################################
# check if a path exists

sub path_exists ($)
{
    if (${FUNC_LOG}) {
		print( "::path_exists : S\n"); }

	my $exists = 0;
	my ($in_path) = @_;

   	if (${DEBUG}) {
		printf( "- path: %s\n", "${in_path}"); }

	if (-e "${in_path}") {
		$exists = 1;
    	if (${DEBUG}) {
			printf( "-- Exists\n"); }
	} else {
    	if (${DEBUG}) {
			printf( "-- Does not exist\n"); }
	}

    if (${FUNC_LOG}) {
		print( "::path_exists : E\n"); }

	return( $exists );
} # path_exists


################################################################################
# We only want to run this script if the previous system version is greater
#  than or equal to 10.4 and less than 10.7!

sub is_valid_version () 
{
    if (${FUNC_LOG}) {
		print("::is_valid_version : S\n"); }

	my ($valid) = 0;

	if ((substr(${g_source_version}, 0, 4) >= ${MIN_VER}) && (substr(${g_source_version}, 0, 4) < ${MAX_VER})) {
		$valid = 1;
    	if (${DEBUG}) {
			printf( "- valid: ${g_source_version}\n");}

		if (substr(${g_source_version}, 0, 4) eq "10.6" or substr(${g_source_version}, 0, 4) eq "10.7") {
			$g_sl_src = 1;
		}
	} else {
		printf ("- Version supplied was not valid: %s\n", $g_source_version);
	}

    if (${FUNC_LOG}) {
		print( "::is_valid_version : E\n"); }

	return( ${valid} );
} # is_valid_version


sub do_dovecot_data_migration ($$)
{
	my($src_path) = $_[0];
	my($dst_path) = $_[1];

	if ( ${FUNC_LOG} ) {
		printf( "::do_dovecot_data_migration : S\n" ); }

	# create a temporary imapd.conf
	if ( path_exists( "${g_imapd_conf}" ) ) {
		qx( ${RM} -rf "${g_imapd_conf}" >> ${MIGRATION_LOG} ); }

	# stuff temp imapd.conf with path to database and default mail store
	#	this is all that is needed for migrating seen flags
	open ( IMAPD_CONF, ">> ${g_imapd_conf}" ) or die("g_imapd_conf: $!\n");
	print IMAPD_CONF "admins: _cyrus\n";
	print IMAPD_CONF "postmaster: postmaster\n";
	print IMAPD_CONF "configdirectory: ${g_db_path}\n";
	print IMAPD_CONF "defaultpartition: default\n";
	print IMAPD_CONF "partition-default: ${src_path}\n";
	close( IMAPD_CONF );

	# are we moving or copying messages
	#	default is to move unless purge is set to 0
	my $cvt_flag = "-m";
	if ( $g_purge == 0 ) {
		$cvt_flag = "-c";
	}

	# migrate default partition
	chdir( "${src_path}/user" ) or die "can't chdir ${src_path}/user: $!";

	my $cyrus_bins;
	if ( path_exists( "${g_source_root}/usr/bin/cyrus/bin" ) ) {
		$cyrus_bins = "${g_source_root}/usr/bin/cyrus/bin";
	} else {
		$cyrus_bins = "/usr/bin/cyrus/bin";
	}

	my @mail_accts = <*>;
	foreach my $user_id (@mail_accts) {
		print LOG_FILE "Migrating user account: ${user_id} to: ${dst_path}\n";

		# get first initial from user account name
		#	used for mapping into cyrus database directory layout
		my $user_tag = substr( $user_id, 0, 1 );

		if ( ${DEBUG} ) {
			printf( "- verifying user seen db: \"${g_db_path}/user/${user_tag}/${user_id}.seen\"\n" ); }

		if ( path_exists( "${g_db_path}/user/${user_tag}/${user_id}.seen" ) ) {
			# Convert from skiplist to flat
			qx( "${cyrus_bins}/cvt_cyrusdb" -C "${g_imapd_conf}" "${g_db_path}/user/${user_tag}/${user_id}.seen" skiplist "${g_db_path}/user/${user_tag}/${user_id}.seen.flat" flat >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );

			# Migrate mail data
			if ( ${DEBUG} ) {
				printf( "-  ${CVT_MAIL_DATA} -g ${cvt_flag} -d ${g_db_path} -s ${src_path} -t ${dst_path} -a ${user_id}\n" );
				qx( ${CVT_MAIL_DATA} -g ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			} else {
				qx( ${CVT_MAIL_DATA} ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			}
			
			# clean up
			qx( ${RM} "${g_db_path}/user/${user_tag}/${user_id}.seen.flat" >> "${MIGRATION_LOG}" );
		} else {
			# Do mail migration without setting any seen flags
			if ( ${DEBUG} ) {
				printf( "-  ${CVT_MAIL_DATA} -g ${cvt_flag} -d ${g_db_path} -s ${src_path} -t ${dst_path} -a ${user_id}\n" );
				qx( ${CVT_MAIL_DATA} -g ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			} else {
				qx( ${CVT_MAIL_DATA} ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			}
		}

		# make sure directory was renamed correctly
		if ( path_exists( "${dst_path}/${user_id}" ) ) {
			print LOG_FILE "Warning: Found user id path: ${dst_path}/${user_id}\n";
			my $i = 0;
			for ( $i = 0; $i < 3; $i++ ) {
				my $a_user_guid = qx( ${CVT_MAIL_DATA} -i ${user_id} );
				chomp( ${a_user_guid} );
				if ( substr(${a_user_guid}, 0, 13) eq "No GUID found" ) {
					sleep(30);
				} else {
					qx( /bin/mv "${dst_path}/${user_id}" "${dst_path}/${a_user_guid}" );
					last;
				}
			}
		}

		# do user id to guid mapping
		my $user_guid = qx( ${CVT_MAIL_DATA} -i ${user_id} );
		chomp( ${user_guid} );
		if ( substr(${user_guid}, 0, 13) eq "No GUID found" ) {
			print LOG_FILE "Warning: No GUID found for user ${user_id} (mailbox path: ${dst_path}/${user_id})\n";
			qx( chown -R _dovecot:mail "${dst_path}/${user_id}" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
		} else {
			qx( chown -R _dovecot:mail "${dst_path}/${user_guid}" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			# set account migration flag
			print LOG_FILE "Setting account flag to migrated for: ${user_id}, (${user_guid})\n";
			qx( ${CVT_MAIL_DATA} -j ${user_guid} );
		}
	}

	# clean up temp imapd.conf
	if ( path_exists( "${g_imapd_conf}" ) ) {
		qx( ${RM} -rf "${g_imapd_conf}" >> ${MIGRATION_LOG} ); }

	if ( ${FUNC_LOG} ) {
		printf( "::do_dovecot_data_migration : S\n" ); }
} # do_dovecot_data_migration


sub do_data_migration()
{
	my $service_state = "SERVICE_DISABLE";

	if (!path_exists("${g_migration_plist}")) {
		print LOG_FILE "No migration data file: ${g_migration_plist}\n";
		return;
	} else {
		copy("${g_migration_plist}", "${g_migration_plist}" . ".default");
	}

	# start mail services on first reboot
	$service_state = qx( ${PLIST_BUDDY} -c 'Print :serviceState' "/etc/MailServicesOther.plist" );
	chomp(${service_state});
	if ( ${service_state} eq "SERVICE_ENABLE" ) {
		qx( ${PLIST_BUDDY} -c 'Set :serviceState string SERVICE_DISABLE' ${g_migration_plist} );
		qx( ${SERVER_ADMIN} start mail >> ${MIGRATION_LOG} );
	}

	$g_source_version = qx(${PLIST_BUDDY} -c 'Print :sourceVersion' ${g_migration_plist});
	chomp(${g_source_version});
	if (!is_valid_version()) {
		print LOG_FILE "Unsupported migration version: ${g_source_version}\n";
		return;
	}

	$g_source_root = qx(${PLIST_BUDDY} -c 'Print :sourceRoot' ${g_migration_plist});
	chomp(${g_source_root});
	my $orig_source_root = $g_source_root;
	my $tries = 0;
	while (!path_exists("${g_source_root}")) {
		print LOG_FILE "Missing or invalid source path: ${g_source_root}\n";

		# maybe the volume was renamed; go look for it by UUID
		my $source_uuid = qx(${PLIST_BUDDY} -c 'Print :sourceUUID' ${g_migration_plist});
		chomp $source_uuid;
		if ($source_uuid =~ /^[0-9A-F]{8}(-[0-9A-F]{4}){3}-[0-9A-F]{12}$/) {
			my @infos = qx(/usr/sbin/diskutil info "$source_uuid");
			my $vol;
			for (@infos) {
				# don't match "/"
				if (/\s*Mount Point:\s*(\/.+)/) {
					$vol = $1;
					last;
				}
			}
			if (defined($vol) && $vol ne $g_source_root && path_exists($vol)) {
				print LOG_FILE "Found volume $vol for source UUID $source_uuid.  Trying that.\n";
				$g_source_root = $vol;
				next;
			}
		}

		return if ++$tries >= 15;
		sleep 60;
	}
	print LOG_FILE "Found source path: ${g_source_root}\n";

	$g_target_root = qx(${PLIST_BUDDY} -c 'Print :targetRoot' ${g_migration_plist});
	chomp(${g_target_root});
	$tries = 0;
	while (!path_exists("${g_target_root}")) {
		print LOG_FILE "Missing or invalid target root: ${g_target_root}\n";
		return if ++$tries >= 15;
		sleep 60;
	}
	print LOG_FILE "Found target path: ${g_target_root}\n";

	$g_purge = qx(${PLIST_BUDDY} -c 'Print :purge' ${g_migration_plist});
	chomp(${g_purge});

	if (${g_sl_src}) {
		return;
	}

	my $db_path = qx(${PLIST_BUDDY} -c 'Print :config_directory' ${g_migration_plist});
	chomp($db_path);
	if ($g_source_root ne $orig_source_root) {
		# if source root changed name, so might the config dir
		$db_path =~ s,^$orig_source_root/,$g_source_root/,;
	}
	if (!defined($db_path) || $db_path eq "") {
		$g_db_path = "$g_source_root/private/var/imap";
	} elsif ($g_source_root eq "/Previous System" && $db_path eq "/var/imap") {
		$g_db_path = "/Previous System/private" . $db_path;
	} elsif ($db_path !~ m,^/Volumes,) {
		$g_db_path = $g_source_root . $db_path;
	} else {
		$g_db_path = $db_path;
	}
	print LOG_FILE "Config path: $g_db_path\n";

	my $default_partition = qx(${PLIST_BUDDY} -c 'Print :default_partition' ${g_migration_plist});
	chomp(${default_partition});
	
	# looking to see if dest is "local" vol or remote
	my $volume_tag = substr( ${default_partition}, 0, 8 );
	my $src_path;
	if ( ("${g_source_root}" eq "/Previous System") && ($default_partition eq "/var/spool/imap") ) {
		$src_path = "/Previous System/private" . ${default_partition};
	} elsif ($volume_tag ne "/Volumes") {
		$src_path = $g_source_root . $default_partition;
	} else {
		$src_path = "${default_partition}";
	}

	# migrate default path
	$tries = 0;
	while (!path_exists("${src_path}/user")) {
		print LOG_FILE "Data store directory does not exist at: ${src_path}/user\n";
		last if ++$tries >= 15;
		sleep 60;
	}
	if (path_exists("${src_path}/user")) {
		my $dst_path;
		if ($default_partition eq "/var/spool/imap") {
			$dst_path = "/Library/Server/Mail/Data/mail";
		} else {
			$dst_path = "${default_partition}/dovecot";
		}

		# make sure destination dirs exist
		if ( ${DEBUG} ) {
			printf( "- verifying data destination directory: \"$dst_path\"\n" ); }

		if ( ! path_exists( "$dst_path" ) ) {
			qx( /bin/mkdir -p -m 775 "$dst_path" >> "${MIGRATION_LOG}" );
			print LOG_FILE "Creating destination directory: $dst_path\n";
		}
		qx( /usr/sbin/chown _dovecot:mail "$dst_path" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );

		do_dovecot_data_migration( "${src_path}", "${dst_path}" );
	} else {
		print LOG_FILE "Error: Data store directory does not exist at: ${src_path}/user\n";
	}

	# migrate any alternate data store locations
	my @alt_paths = qx(${PLIST_BUDDY} -c 'Print :alternate_partitions' ${g_migration_plist});
	if ($#alt_paths > 1) {
		for (my $i = 1; $i < $#alt_paths; $i++) {
			my $a_partition = $alt_paths[$i];
			chomp(${a_partition});
			$a_partition =~ s/^\s*//;

			$volume_tag = substr( $a_partition, 0, 8 );
			if ( $volume_tag eq "/Volumes" ) {
				$src_path = "$a_partition";
			} else {
				# if source root is from previous system and not on
				#	a remote volume, then data store is local
				if ( "${g_source_root}" eq "/Previous System" ) {
					$src_path = "${a_partition}";
				} else {
					$src_path = "${g_source_root}${a_partition}";
				}
				push( @g_clean_partitions, "${src_path}" );
			}

			# verify that the source directories are accessible
			if ( ${DEBUG} ) {
				printf( "- verifying data store directory: \"${src_path}/user\"\n" ); }

			$tries = 0;
			while (!path_exists("${src_path}/user")) {
				print LOG_FILE "Data store directory does not exist at: ${src_path}/user\n";
				last if ++$tries >= 15;
				sleep 60;
			}
			if( path_exists( "${src_path}/user" ) ) {
				# make sure destination dirs exist
				if ( ${DEBUG} ) {
					printf( "- verifying data destination directory: \"${a_partition}/dovecot\"\n" ); }

				if( !path_exists("${a_partition}/dovecot")) {
					qx(/bin/mkdir "${a_partition}/dovecot" >> "${MIGRATION_LOG}");
				}
				qx(/usr/sbin/chown _dovecot:mail "${a_partition}/dovecot" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}");

				# migrate default partition
				print LOG_FILE "Migrating alternate mail partition: ${src_path} to: ${a_partition}/dovecot\n";

				do_dovecot_data_migration("${src_path}", "${a_partition}/dovecot");
			} else {
				print LOG_FILE "Error: Data store directory does not exist at: ${src_path}/user\n";
			}
		}
	}

	print LOG_FILE "End of migration\n";
}


