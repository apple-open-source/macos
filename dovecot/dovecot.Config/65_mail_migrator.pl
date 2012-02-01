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

use strict;

## Migration Script for Mail Services

##################   Input Parameters  #######################
# --purge <0 | 1>   "1" means remove any files from the old system after you've migrated them, "0" means leave them alone.
# --sourceRoot <path> The path to the root of the system to migrate
# --sourceType <System | TimeMachine> Gives the type of the migration source, whether it's a runnable system or a
#                  Time Machine backup.
# --sourceVersion <ver> The version number of the old system (like 10.5.7 or 10.6). Since we support migration from 10.5, 10.6,
#                   and other 10.7 installs.
# --targetRoot <path> The path to the root of the new system.
# --language <lang> A language identifier, such as \"en.\" Long running scripts should return a description of what they're doing
#                   (\"Migrating Open Directory users\"), and possibly provide status update messages along the way. These messages
#                   need to be localized (which is not necessarily the server running the migration script).
#                   This argument will identify the Server Assistant language. As an alternative to doing localization yourselves
#                   send them in English, but in case the script will do this it will need this identifier.
#
# Example: /System/Library/ServerSetup/MigrationExtras/65_mail_migrator.pl --purge 0 --language en --sourceType System --sourceVersion 10.5 --sourceRoot "/Previous System" --targetRoot "/"

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
my $SERVER_ADMIN = "/usr/sbin/serveradmin";
my $POSTCONF = "/usr/sbin/postconf";
my $MKDIR = "/bin/mkdir";
my $PLIST_BUDDY = "/usr/libexec/PlistBuddy";
my $CVT_MAIL_DATA = "/usr/bin/cvt_mail_data";
my $TAR = "/usr/bin/tar";

################################## Consts ###################################
my $IMAPD_CONF="/private/etc/imapd.conf";
my $MIGRATION_LOG= "/Library/Logs/MailMigration.log";

############################## Version Consts  ##############################
my $SYS_VERS	= "0";	#10.5.7
my $SYS_MAJOR	= "0";	#10
my $SYS_MINOR	= "0";	# 5
my $SYS_UPDATE	= "-";	# 7
my $SRV_VERS	= "0";	#10.5.7
my $SRV_MAJOR	= "0";	#10
my $SRV_MINOR	= "0";	# 5
my $SRV_UPDATE	= "-";	# 7
my $MIN_VER		= "10.5"; # => 10.5
my $MAX_VER		= "10.8"; # <  10.8

my $TARGET_VER = "10.7";

################################### Paths ###################################
my $HOSTCONFIG		= "/private/etc/hostconfig";

# amavisd-new
my $g_amavisd_launchd_plist			= "/System/Library/LaunchDaemons/org.amavis.amavisd.plist";
my $g_amavisd_cleanup_launchd_plist	= "/System/Library/LaunchDaemons/org.amavis.amavisd_cleanup.plist";
# Clam AV
my $g_clamav_launchd_plist			= "/System/Library/LaunchDaemons/org.clamav.clamd.plist";
my $g_freshclam_launchd_plist		= "/System/Library/LaunchDaemons/org.clamav.freshclam.plist";
# SpamAssassin
my $g_updatesa_launchd_plist		= "/System/Library/LaunchDaemons/com.apple.updatesa.plist";
my $g_learn_junk_launchd_plist		= "/System/Library/LaunchDaemons/com.apple.learnjunkmail.plist";	# pre 10.7
my $g_salearn_launchd_plist			= "/System/Library/LaunchDaemons/com.apple.salearn.plist";			# 10.7+
# dovecot
my $g_dovecot_launchd_plist			= "/System/Library/LaunchDaemons/org.dovecot.dovecotd.plist";
my $g_push_notify_launchd_plist		= "/System/Library/LaunchDaemons/com.apple.push_notify.plist";
my $g_fts_update_launchd_plist		= "/System/Library/LaunchDaemons/org.dovecot.fts.update.plist";
my $g_dovecot_sieve_launchd_plist	= "/System/Library/LaunchDaemons/com.apple.wiki_sieve_manager.plist";
# mailman
my $g_mailman_launchd_plist			= "/System/Library/LaunchDaemons/org.list.mailmanctl.plist";
# postfix
my $g_postfix_launchd_plist			= "/System/Library/LaunchDaemons/org.postfix.master.plist";

################################### Keys ####################################
my $g_postfix_launchd_key	= "org.postfix.master";
my $g_cyrus_launchd_key		= "edu.cmu.andrew.cyrus.master";
my $g_mailman_launchd_key	= "org.list.mailmanctl";

################################## Globals ##################################
my $g_purge			= 0;		# So we will be default copy the items if
								#there's no option specified.
my $g_source_root		= "";
my $g_source_type		= "";
my $g_source_version	= "";	# This is the version number of the old system
								# passed into us by Server Assistant.
								# [10.5.x, 10.6.x, and potentially 10.7.x]
my $g_source_uuid		= "";
my $g_target_root		= "";
my $g_language			= "en";	# Should be Tier-0 only in iso format
								# [en, fr, de, ja], we default this to English, en.
my $g_xsan_volume		= "null";
my $g_dovecot_ssl_key	= "";
my $g_dovecot_ssl_cert	= "";
my $g_dovecot_ssl_ca	= "/etc/certs/Default.csr";
my $g_postfix_ssl_key	= "";
my $g_postfix_ssl_cert	= "";
my $g_postfix_ssl_ca	= "";
my $g_luser_relay		= 0;

my $g_cyrus_enabled		= 0;
my $g_mailman_enabled	= 0;

my @g_partitions		= ();
my @g_clean_partitions	= ();
my $g_default_partition	= "/var/spool/imap";
my $g_db_path			= "/private/var/imap";
my $g_imapd_conf		= "/tmp/imapd.conf.tmp";
my $g_migration_plist	= "/var/db/.mailmigration.plist";
my $g_migration_ld_plist= "/System/Library/LaunchDaemons/com.apple.mail_migration.plist";

my $g_enable_spam		= 0;
my $g_enable_virus		= 0;

my $g_sl_src			= 0;	# Set to 1 if source is Snow Leopard Server
my $g_postfix_root		= "/";

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

if ( $ENV{FUNC_LOG} eq 1 ) {
	$FUNC_LOG = '1'; }

parse_options();

if ( ${DEBUG} ) {
	print_script_args( @ARGV ); }

open (LOG_FILE, ">> ${MIGRATION_LOG}") or die("$MIGRATION_LOG: $!\n");

do_migration( @ARGV );

exit();


################################# Functions #################################

#################################################################
# print_message ()

sub print_message ()
{
	my ( $line_0 ) = $_[0];
	my ( $line_1 ) = $_[1];
	my ( $line_2 ) = $_[2];
	my ( $line_3 ) = $_[3];

	if ( ${FUNC_LOG} ) {
		printf( "::print_message : S\n" ); }

	print LOG_FILE "*************************************************************\n";
	if ( ! ("${line_0}" eq "") ) {
		print LOG_FILE "** ${line_0}\n"; }
	if ( ! ("${line_1}" eq "") ) {
		print LOG_FILE "** ${line_1}\n"; }
	if ( ! ("${line_2}" eq "") ) {
		print LOG_FILE "** ${line_2}\n"; }
	if ( ! ("${line_3}" eq "") ) {
		print LOG_FILE "** ${line_3}\n"; }
	print LOG_FILE "** Please refer to the Migration and Upgrade Guide for\n";
	print LOG_FILE "** instructions on how to manually migrate configuration data.\n";
	print LOG_FILE "*************************************************************\n";

	if ( ${FUNC_LOG} ) {
		printf( "::print_message : E\n" ); }
} # print_message


################################################################################
# plist methods

sub obj_value
{
  my ( $object ) = @_;
  return( $object->description()->UTF8String() );
} # obj_value


#################################################################
# map_ssl_cert ()
#
#	: map default certificate from 10.5 to 10.6/7 certificates

sub map_ssl_cert ($)
{
	my( $in_cert ) = @_;

	if ( ${FUNC_LOG} ) {
		printf( "::map_ssl_cert : S\n" ); }

	if ( ${DEBUG} ) {
		print( "Mapping certificate: ${in_cert}\n" ); }

	# we will only migrate certificates from /etc/certificates
	if ( substr( ${in_cert}, 0, 18) eq "/etc/certificates/" )
	{
		# get the certificate name by removing path
		my $cert_name = substr( ${in_cert}, 18 );

		# ket suffix so we can map key or cert
		my $cert_suffix = substr( ${cert_name}, -4 );

		# remove the .key or .crt suffix for name mapping
		$cert_name = substr( ${cert_name}, 0, length(${cert_name})-4);

		# map from current certificates directory
		chdir( "/private/etc/certificates" );

		my @mail_certs = <*>;
		foreach my $a_cert ( @mail_certs )
		{
			chomp($a_cert);

			# looking for key
			if ( ${cert_suffix} eq ".key" )
			{
				if ( ((substr( ${a_cert}, 0, length(${cert_name})) eq ${cert_name})) && (index($a_cert, ".key.pem") != -1) )
				{
					print LOG_FILE "Mapping certificate: ${in_cert} to: ${a_cert}\n";
					return($a_cert);
				}
			}
			elsif ( ${cert_suffix} eq ".crt" )
			{
				if ( ((substr( ${a_cert}, 0, length(${cert_name})) eq ${cert_name})) && (index($a_cert, ".cert.pem") != -1) )
				{
					print LOG_FILE "Mapping certificate: ${in_cert} to: ${a_cert}\n";
					return($a_cert);
				}
			}
			elsif ( ${cert_suffix} eq ".csr" )
			{
				if ( ((substr( ${a_cert}, 0, length(${cert_name})) eq ${cert_name})) && (index($a_cert, ".chain.pem") != -1) )
				{
					print LOG_FILE "Mapping certificate: ${in_cert} to: ${a_cert}\n";
					return($a_cert);
				}
			} else {
				print LOG_FILE "Unknown certificate type:" . ${in_cert} ."\n";
			}
		}
	}

	if ( ${FUNC_LOG} ) {
		printf( "::map_ssl_cert : E\n" ); }

	# Not good, no certificate found in /etc/certificates that matches
	#	configuration settings from previous server
	print LOG_FILE "Warning: No certificate map found for: " . ${in_cert} ."\n";

	return( "" );

} # map_ssl_cert


#################################################################
# get_ssl_certs ()

sub get_ssl_certs ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::get_ssl_certs : S\n" ); }

	# cert migration not necessary from 10.6/7 to 10.6.x/10.7
	if ( $g_sl_src ) {
		return;
	}

	# get certs from cyrus imapd.conf
	#	do -not- check for open failure, certs will simply not get migrated
	#	if /etc/impad.conf does not exist
	open(CYRUS_CONFIG, "<${g_source_root}" . ${IMAPD_CONF});
	while( <CYRUS_CONFIG> )
	{
		# store $_ value because subsequent operations may change it
		my( $config_key ) = $_;
		my( $config_value ) = $_;

		# strip the trailing newline from the line
		chomp($config_key);
		chomp($config_value);

		if ( $config_key =~ s/:.*// )
		{
			if ( $config_value =~ s/^.*:// )
			{
				# trim whitespace
				$config_value =~ s/^\s+//;
				$config_value =~ s/\s+$//;

				SWITCH: {
					if ( $config_key eq "tls_key_file" )
					{
						$g_dovecot_ssl_key = map_ssl_cert( ${config_value} );
						last SWITCH;
					}
					if ( $config_key eq "tls_cert_file" )
					{
						$g_dovecot_ssl_cert = map_ssl_cert( ${config_value} );
						last SWITCH;
					}
					if ( $config_key eq "tls_ca_file" )
					{
						$g_dovecot_ssl_ca = map_ssl_cert( ${config_value} );
						last SWITCH;
					}
					last SWITCH;
				}
			}
		}
	}

	# close it, close it up again
	close( CYRUS_CONFIG );

	# get postfix certs
	my @smtp_key = qx( ${POSTCONF} -h -c "${g_postfix_root}/private/etc/postfix" smtpd_tls_key_file );
	chomp(@smtp_key);
	$g_postfix_ssl_key = map_ssl_cert( $smtp_key[0] );

	my @smtp_cert = qx( ${POSTCONF} -h -c "${g_postfix_root}/private/etc/postfix" smtpd_tls_cert_file );
	chomp(@smtp_cert);
	$g_postfix_ssl_cert = map_ssl_cert( $smtp_cert[0] );

	my @smtp_ca = qx( ${POSTCONF} -h -c "${g_postfix_root}/private/etc/postfix" smtpd_tls_CAfile );
	chomp(@smtp_ca);
	$g_postfix_ssl_ca = map_ssl_cert( $smtp_ca[0] );

	# sanity check
	# if the cert and key are set but chained is not, look for valid chained cert and set it.
	if ( !(${g_dovecot_ssl_key} eq "") && !(${g_dovecot_ssl_cert} eq "") )
	{
		if ( ${g_dovecot_ssl_ca} eq "" || $g_dovecot_ssl_ca	eq "/etc/certs/Default.csr" )
		{
			my $offset = index(${g_dovecot_ssl_key}, ".key.pem");
			if ( $offset != -1 )
			{
				# this will ket the cert name up to .key.pem
				$g_dovecot_ssl_ca = substr( ${g_dovecot_ssl_key}, 0, ${offset} );
				$g_dovecot_ssl_ca = ${g_dovecot_ssl_ca} . ".chain.pem";
			}
		}
	}

	if ( !(${g_postfix_ssl_key} eq "") && !(${g_postfix_ssl_cert} eq "") )
	{
		if ( ${g_postfix_ssl_ca} eq "" )
		{
			my $offset = index(${g_postfix_ssl_key}, ".key.pem");
			if ( $offset != -1 )
			{
				# this will ket the cert name up to .key.pem
				$g_postfix_ssl_ca = substr( ${g_postfix_ssl_key}, 0, ${offset} );
				$g_postfix_ssl_ca = ${g_postfix_ssl_ca} . ".chain.pem";
			}
		}
	}

	if ( ${FUNC_LOG} ) {
		printf( "::get_ssl_certs : E\n" ); }
} # get_ssl_certs


#################################################################
# migrate_db_update_times ()

sub migrate_db_update_times ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_db_update_times : S\n" ); }

	print LOG_FILE "Migrating virus database upgrade interval\n";

	open(CLAMAV_PLIST, "<${g_source_root}" . "/System/Library/LaunchDaemons/org.clamav.freshclam.plist");
	while( <CLAMAV_PLIST> )
	{
		# store $_ value because subsequent operations may change it
		my( $line ) = $_;

		# strip the trailing newline from the line
		chomp( $line );

		my $key = index($line, "-c ");
		if ( ${key} != -1 )
		{
			my $value;
			if ( substr( ${line}, ${key}+4, 1 ) eq "<" )
			{
				$value = substr( ${line}, ${key}+3, 1 );
			} else {
				$value = substr( ${line}, ${key}+3, 2 );
			}
			qx( ${SERVER_ADMIN} settings mail:postfix:virus_db_update_days = ${value} >> ${MIGRATION_LOG} );
		}
	}

	close( CLAMAV_PLIST );

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_db_update_times : E\n" ); }
} # migrate_db_update_times


#################################################################
# migrate_log_settings ()

sub migrate_log_settings ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_log_settings : S\n" ); }

	# look for syslog.conf in source-root or in target-root with ~previous extension
	my $syslog_conf_path = ${g_target_root} . "/private/etc/syslog.conf~previous";
	if ( !path_exists( ${syslog_conf_path} ) ) {
		print LOG_FILE "Note: previous syslog.conf: " . ${syslog_conf_path} . ", not found\n";

		$syslog_conf_path = ${g_source_root} . "/private/etc/syslog.conf";
		if ( !path_exists( ${syslog_conf_path} ) ) {
			print LOG_FILE "Note: previous syslog.conf: " . ${syslog_conf_path} . ", not found\n";
			print LOG_FILE "Log level settings not migrated\n";
			return;
		}
	}

	print LOG_FILE "Migrating log level settings from: ${syslog_conf_path}\n";

	# get mail. & local6. log settings
	open(SYS_LOG, "<${syslog_conf_path}");
	while( <SYS_LOG> )
	{
		# store $_ value because subsequent operations may change it
		my( $line ) = $_;

		# strip the trailing newline from the line
		chomp( $line );

		my $offset = 0;
		my $value = "";
		if ( substr( ${line}, 0, 5) eq "mail." )
		{
			${offset} = 5;
		}
		elsif (substr( ${line}, 0, 7) eq "local6." )
		{
			${offset} = 7;
		}

		if ( ${offset} != 0 )
		{
			SWITCH: {
				if ( substr( ${line}, ${offset}, 3) eq "err" )
				{
					${value} = "err";
					last SWITCH;
				}
				if ( substr( ${line}, ${offset}, 4) eq "crit" )
				{
					${value} = "crit";
					last SWITCH;
				}
				if ( substr( ${line}, ${offset}, 4) eq "warn" )
				{
					${value} = "warn";
					last SWITCH;
				}
				if ( substr( ${line}, ${offset}, 6) eq "notice" )
				{
					${value} = "notice";
					last SWITCH;
				}
				if ( substr( ${line}, ${offset}, 4) eq "info" )
				{
					${value} = "info";
					last SWITCH;
				}
				if ( substr( ${line}, ${offset}, 1) eq "*" )
				{
					${value} = "debug";
					last SWITCH;
				}
				last SWITCH;
			}
			if ( !(${value} eq "") )
			{
				if ( ${offset} == 5 )
				{
					qx( ${SERVER_ADMIN} settings mail:postfix:log_level = ${value} >> ${MIGRATION_LOG} );
				}
				elsif  ( ${offset} == 7 )
				{
					qx( ${SERVER_ADMIN} settings mail:imap:log_level = ${value} >> ${MIGRATION_LOG} );
				}
			}
		}
	}

	# close it, close it up again
	close( SYS_LOG );

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_log_settings : E\n" ); }
} # migrate_log_settings


#################################################################
# get_services_state ()

sub get_services_state ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::get_services_state : S\n" ); }

	# the response to launchctl list is: "launchctl list returned unknown response"
	# if the service is not running, otherwise it returns the full launchd plist

	# check cyrus imap state
	my $TEST = qx( ${LAUNCHCTL} list $g_cyrus_launchd_key >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	if ( ! (substr( $TEST, 0, 9) eq "launchctl") )
	{
		if ( ${DEBUG} ) {
			print( "Cyrus enabled:\n $TEST\n" ); }

		$g_cyrus_enabled = 1;
	}

	# check mailman state
	$TEST = qx( ${LAUNCHCTL} list $g_mailman_launchd_key >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	if ( ! (substr( $TEST, 0, 9) eq "launchctl") )
	{
		if ( ${DEBUG} ) {
			print( "Mailman enabled:\n $TEST\n" ); }

		$g_mailman_enabled = 1;
	}

	if ( ${FUNC_LOG} ) {
		printf( "::get_services_state : E\n" ); }
} # get_services_state


#################################################################
# halt_mail_services ()

sub halt_mail_services ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::halt_mail_services : S\n" ); }

	print LOG_FILE "Halting mail services\n";

	# stop amavisd
	if ( path_exists("${g_amavisd_launchd_plist}") ) {
		if ( ${DEBUG} ) {
			print( "- Stopping amavisd\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_amavisd_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	if ( path_exists("${g_amavisd_cleanup_launchd_plist}") ) {
		if ( ${DEBUG} ) {
			print( "- Stopping amavisd cleanup\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_amavisd_cleanup_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	# stop clamav
	if ( path_exists( "${g_clamav_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping clamd\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_clamav_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	if ( path_exists( "${g_freshclam_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping freshclam\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_freshclam_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	# stop SpamAssassin
	if ( path_exists( "${g_updatesa_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping sa-update\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_updatesa_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	if ( path_exists( "${g_salearn_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping sa-learn\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_salearn_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	# stop mailman
	if ( path_exists( "${g_mailman_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping mailman\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_mailman_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	# stop postfix
	if ( path_exists( "${g_postfix_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping postfix\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_postfix_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	# stop dovecot
	if (path_exists($g_fts_update_launchd_plist)) {
		if ( ${DEBUG} ) {
			print( "- Stopping update-fts-index\n" ); }

		qx(${LAUNCHCTL} unload -w ${g_fts_update_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}");
	}

	if ( path_exists( "${g_push_notify_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping push notification\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_push_notify_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	if ( path_exists( "${g_dovecot_sieve_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping dovecot sieve\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_dovecot_sieve_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	if ( path_exists( "${g_dovecot_launchd_plist}" ) ) {
		if ( ${DEBUG} ) {
			print( "- Stopping dovecot\n" ); }

		qx( ${LAUNCHCTL} unload -w ${g_dovecot_launchd_plist} >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	if ( ${FUNC_LOG} ) {
		printf( "::halt_mail_services : E\n" ); }

} # halt_mail_services


#################################################################
# copy_plists ()

sub copy_plists ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::copy_plists : S\n" ); }

	print LOG_FILE "Halting mail services\n";

	# copy amavisd
	if ( path_exists("${g_source_root}" . "${g_amavisd_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_amavisd_launchd_plist}", "${g_amavisd_launchd_plist}" );
	}
	if ( path_exists("${g_source_root}" . "${g_amavisd_cleanup_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_amavisd_cleanup_launchd_plist}", "${g_amavisd_cleanup_launchd_plist}" );
	}

	# stop clamav
	if ( path_exists("${g_source_root}" . "${g_clamav_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_clamav_launchd_plist}", "${g_clamav_launchd_plist}" );
	}
	if ( path_exists("${g_source_root}" . "${g_freshclam_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_freshclam_launchd_plist}", "${g_freshclam_launchd_plist}" );
	}

	# stop SpamAssassin
	if ( path_exists("${g_source_root}" . "${g_updatesa_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_updatesa_launchd_plist}", "${g_updatesa_launchd_plist}" );
	}
	if ( path_exists("${g_source_root}" . "${g_salearn_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_salearn_launchd_plist}", "${g_salearn_launchd_plist}" );
	}

	# stop mailman
	if ( path_exists("${g_source_root}" . "${g_mailman_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_mailman_launchd_plist}", "${g_mailman_launchd_plist}" );
	}

	# stop postfix
	if ( path_exists("${g_source_root}" . "${g_postfix_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_postfix_launchd_plist}", "${g_postfix_launchd_plist}" );
	}

	# stop dovecot
	if ( path_exists("${g_source_root}" . "${g_fts_update_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_fts_update_launchd_plist}", "${g_fts_update_launchd_plist}" );
	}
	if ( path_exists("${g_source_root}" . "${g_push_notify_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_push_notify_launchd_plist}", "${g_push_notify_launchd_plist}" );
	}
	if ( path_exists("${g_source_root}" . "${g_dovecot_sieve_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_dovecot_sieve_launchd_plist}", "${g_dovecot_sieve_launchd_plist}" );
	}
	if ( path_exists("${g_source_root}" . "${g_dovecot_launchd_plist}") ) {
		copy("${g_source_root}" . "${g_dovecot_launchd_plist}", "${g_dovecot_launchd_plist}" );
	}

	if ( ${FUNC_LOG} ) {
		printf( "::copy_plists : E\n" ); }

} # copy_plists


################################################################################
# get old server version parts

sub get_server_version ($)
{
	my ($VERS) = @_;
	if ( ${FUNC_LOG} ) {
		print( "::get_server_version : S\n"); }

	if ( ${DEBUG} ) {
		printf( "- sourceVersion: %s\n", "${VERS}"); }

	my @SRV_VER_PARTS = split(/\./, $VERS); 
	if ( ${DEBUG} )
	{
		print( "- Major : " . ${SRV_VER_PARTS}[0] . "\n" );
		print( "- Minor : " . ${SRV_VER_PARTS}[1] . "\n" );
		print( "- Update: " . ${SRV_VER_PARTS}[2] . "\n" );
	}

	$SRV_MAJOR = ${SRV_VER_PARTS}[0];
	$SRV_MINOR = ${SRV_VER_PARTS}[1];
	$SRV_UPDATE = ${SRV_VER_PARTS}[2];

	if (${FUNC_LOG}) {
		print( "::get_server_version : E\n"); }
} # get_server_version


#################################################################
# get_mail_partitions ()

sub get_mail_partitions ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::get_mail_partitions : S\n" ); }

	if ( ${DEBUG} ) {
		print( "- opening: ${g_source_root} . ${IMAPD_CONF}\n" ); }

	#	do -not- check for open failure, custom mail partitions will
	#	 simply not get migrated if /etc/impad.conf does not exist
	open( cyrus_imapd, "<${g_source_root}" . ${IMAPD_CONF} );
	while( <cyrus_imapd> )
	{
		# store $_ value because subsequent operations may change it
		my( $config_key ) = $_;
		my( $data_path ) = $_;

		# strip the trailing newline from the line
		chomp($config_key);
		chomp($data_path);

		if ( $config_key =~ s/:.*// )
		{
			if ( $data_path =~ s/^.*:// )
			{
				# trim whitespace
				$data_path =~ s/^\s+//;
				$data_path =~ s/\s+$//;

				my $partition_tag = substr( $config_key, 0, 10 );
				if ( $partition_tag eq "partition-" )
				{
					$partition_tag = substr( $config_key, 10, (length($config_key) - 10) );
					if ( "$partition_tag" eq "default" )
					{
						# only do migration on non-Xsan volumes
						if ( ! (substr( "${data_path}", 0, length($g_xsan_volume)) eq $g_xsan_volume) ) {
							$g_default_partition = "${data_path}";
						} else {
							&print_message( "Warning:", "Mail data was located on an Xsan volume:", "  ${data_path}", "This data will need to be migrated manually" );
						}
						if ( ${DEBUG} ) {
							printf( "- default partition: \"${data_path}\n" ); }
					} else {
						# only do migration on non-Xsan volumes
						if ( ! (substr( "${data_path}", 0, length($g_xsan_volume)) eq $g_xsan_volume) ) {
							push( @g_partitions, "${data_path}" );
						} else {
							&print_message( "Warning:", "Mail data was located on an Xsan volume:", "  ${data_path}", "This data will need to be migrated manually" );
						}
						if ( ${DEBUG} ) {
							printf( "- alt partition: \"${data_path}\"\n" ); }
					}
				}
			}
		}
	}

	if ( ${FUNC_LOG} ) {
		printf( "::get_mail_partitions : E\n" ); }

	close( cyrus_imapd );

} # get_mail_partitions


#################################################################
# do_xsan_check ()

sub do_xsan_check ()
{
	if (${FUNC_LOG}) {
		print( "::do_xsan_check : S\n"); }

	my $MOUNTS_TXT = "/private/etc/dovecot/mount.txt";

	qx( mount -v > ${MOUNTS_TXT} );

	open( VOLUMES, "< ${MOUNTS_TXT}" ) or die "can't open ${MOUNTS_TXT}: $!";
	while( <VOLUMES> )
	{
		# store $_ value because subsequent operations may change it
		my( $TYPE ) = $_;
		my( $VOL_NAME ) = $_;

		# strip the trailing newline from the line
		chomp( $TYPE );
		chomp( $VOL_NAME );

		if ( ${DEBUG} ) {
			printf( "- Volume: %s\n", ${VOL_NAME}); }

		# get the volume type from mount text:
		#	ie. /dev/disk0s4 on /Volumes/Leopard-GM (hfs, local, journaled)

		$TYPE =~ s/^.*\(//;
		$TYPE =~ s/,.*//;

		# if it is of acfs type, then is an Xsan volume
		if ( ${TYPE} eq "acfs" )
		{
			if ( ${DEBUG} ) {
				printf( "- Xsan volume: %s\n", ${VOL_NAME}); }

			# get the volume name
			$VOL_NAME =~ s/^.*on //;
			$VOL_NAME =~ s/ .*//;
			$g_xsan_volume = $VOL_NAME;

			if ( ${DEBUG} ) {
				printf( "- Xsan volume name: %s\n", ${g_xsan_volume}); }

			# only 1 xsan supported so break if we find 1
			last;
		}
	}

	close( VOLUMES );

	if ( path_exists("${MOUNTS_TXT}") ) {
		unlink("${MOUNTS_TXT}"); }

	if (${FUNC_LOG}) {
		print( "::do_xsan_check : E\n"); }
} # do_xsan_check


#################################################################
# do_cyrus_db_cleanup ()

sub do_cyrus_db_cleanup ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_db_cleanup : S\n" ); }

	# nothing to clean up if in-place-upgrade
	if ( "${g_source_root}" eq "/Previous System" )
	{
		$g_db_path = "/Previous System/private/var/imap";
		return( 0 );
	}

	my $exit_code = 1;

	if ( ${DEBUG} ) {
		print( "- opening: ${g_source_root} . ${IMAPD_CONF}\n" ); }

	open( cyrus_imapd, "<${g_source_root}" . ${IMAPD_CONF} );
	while( <cyrus_imapd> )
	{
		# store $_ value because subsequent operations may change it
		my( $config_key ) = $_;
		my( $config_value ) = $_;

		# strip the trailing newline from the line
		chomp($config_key);
		chomp($config_value);

		if ( ${DEBUG} ) {
			print( "- line: $config_key\n" ); }

		if ( $config_key =~ s/:.*// )
		{
			if ( $config_key eq "configdirectory" )
			{
				if ( $config_value =~ s/^.*:// )
				{
					# trim whitespace
					$config_value =~ s/^\s+//;
					$config_value =~ s/\s+$//;

					# set global cyrus database path
					my $volume_tag = substr( $config_value, 0, 8 );
					if ( ${volume_tag} eq "/Volumes" )
					{
						$g_db_path = "$config_value";
					} else {
						$g_db_path = "${g_source_root}${config_value}";
					}

					if ( path_exists( "${g_db_path}" ) )
					{
						# copy cyrus db to /var/imap
						qx( ${CP} -rpfv "${g_db_path}" "/private/var/imap" 2>&1 >> ${MIGRATION_LOG} );

						if ( ${DEBUG} ) {
							print( "- copy user database from: ${g_db_path}\n" ); }

						$exit_code = 0;
					} else {
						&print_message( "Error:", "Missing cyrus database: ${g_db_path}" );
					}
				}
			}
		}
	}

	close( cyrus_imapd );

	if ( $exit_code != 0 ) {
		&print_message( "Error:", "Missing configdirectory key in ${g_source_root}" ); }

	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_db_cleanup : E\n" ); }

	return( $exit_code );

} # do_cyrus_db_cleanup


#################################################################
# do_dovecot_data_migration ()
#
#  Note: Routine has been deprecated 

sub do_dovecot_data_migration ($)
{
	my( $src_path ) = $_[0];
	my( $dst_path ) = $_[1];

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
	foreach my $user_id (@mail_accts)
	{
		print LOG_FILE "Migrating user account: ${user_id} to: ${dst_path}\n";

		# get first initial from user account name
		#	used for mapping into cyrus database directory layout
		my $user_tag = substr( $user_id, 0, 1 );

		if ( ${DEBUG} ) {
			printf( "- verifying user seen db: \"${g_db_path}/user/${user_tag}/${user_id}.seen\"\n" ); }

		if ( path_exists( "${g_db_path}/user/${user_tag}/${user_id}.seen" ) )
		{
			# Convert from skiplist to flat
			qx( "${cyrus_bins}/cvt_cyrusdb" -C "${g_imapd_conf}" "${g_db_path}/user/${user_tag}/${user_id}.seen" skiplist "${g_db_path}/user/${user_tag}/${user_id}.seen.flat" flat >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );

			# Migrate mail data
			if ( ${DEBUG} )
			{
				printf( "-  /usr/bin/cvt_mail_data -g ${cvt_flag} -d ${g_db_path} -s ${src_path} -t ${dst_path} -a ${user_id}\n" );
				qx( /usr/bin/cvt_mail_data -g ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			} else {
				qx( /usr/bin/cvt_mail_data ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			}
			
			# clean up
			qx( ${RM} "${g_db_path}/user/${user_tag}/${user_id}.seen.flat" >> "${MIGRATION_LOG}" );
		} else {
			# Do mail migration without setting any seen flags
			if ( ${DEBUG} )
			{
				printf( "-  /usr/bin/cvt_mail_data -g ${cvt_flag} -d ${g_db_path} -s ${src_path} -t ${dst_path} -a ${user_id}\n" );
				qx( /usr/bin/cvt_mail_data -g ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			} else {
				qx( /usr/bin/cvt_mail_data ${cvt_flag} -d "${g_db_path}" -s "${src_path}" -t "${dst_path}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			}
		}

		# do user id to guid mapping
		my $user_guid = qx( /usr/bin/cvt_mail_data -i ${user_id} );
		chomp( ${user_guid} );
		if ( substr(${user_guid}, 0, 13) eq "No GUID found" ) {
			qx( chown -R _dovecot:mail "${dst_path}/${user_id}" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
		} else {
			qx( chown -R _dovecot:mail "${dst_path}/${user_guid}" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
		}
	}

	# clean up temp imapd.conf
	if ( path_exists( "${g_imapd_conf}" ) ) {
		qx( ${RM} -rf "${g_imapd_conf}" >> ${MIGRATION_LOG} ); }

	if ( ${FUNC_LOG} ) {
		printf( "::do_dovecot_data_migration : S\n" ); }

} # do_dovecot_data_migration


#################################################################
# do_cyrus_dovecot_migration ()

sub do_cyrus_dovecot_migration ()
{
	my $INDEX=0;

	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_dovecot_migration : S\n" ); }
 
	# get partitions from imapd.conf
	get_mail_partitions();

	# set partition paths to be migrated later
	qx( ${PLIST_BUDDY} -c 'Add :sourceRoot string ${g_source_root}' ${g_migration_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :config_directory string ${g_db_path}' ${g_migration_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :default_partition string ${g_default_partition}' ${g_migration_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :alternate_partitions array' ${g_migration_plist} );

	# set alt partition info to migration plist
	foreach ( @g_partitions ) {
		my( $a_partition ) = $_;
		qx( ${PLIST_BUDDY} -c 'Add :alternate_partitions:${INDEX} string ${a_partition}' ${g_migration_plist} );
		${INDEX} = ${INDEX} + 1;
	}

	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_dovecot_migration : E\n" ); }

} # do_cyrus_dovecot_migration


#################################################################
# do_cyrus_config_check ()

sub do_cyrus_config_check ($)
{
	my ( $arg_1 ) = @_;

	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_config_check : S\n" ); }

	if ( ${DEBUG} ) {
		printf( "- Checking for: %s\n", "${g_source_root}" . "${IMAPD_CONF}" ); }

	if ( ! path_exists( "${g_source_root}" . "${IMAPD_CONF}" ) )
	{
		&print_message( "Error:", "Missing configuration file: ${g_source_root}" . "${IMAPD_CONF}", "No ${$arg_1} data was migrated." );

		if ( ${FUNC_LOG} ) {
			printf( "::do_cyrus_config_check : E - 1\n" ); }

		return( 1 );
	}

	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_config_check : E - 0\n" ); }

	return( 0 );
} # do_cyrus_config_check


#################################################################
# migrate_cyrus_config ()

sub migrate_cyrus_config ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_cyrus_config : S\n" ); }

	if ( do_cyrus_config_check( "configuration" ) )
	{
		if ( ${FUNC_LOG} ) {
			printf( "::migrate_cyrus_config : 1: 1\n" ); }

		return;
	}

	#################################################################
	# Setup temp partition file
	my $clear_auth_enabled = "yes";
	my $quota_warn_value = 90;	# 10.5 default
	my $INDEX=0;
	my $TMP_FILE="/private/etc/dovecot/tmp-partitions.txt";
	if ( path_exists("${TMP_FILE}") ) {
		unlink("${TMP_FILE}"); }

	open( CYRUS_CONFIG, "<${g_source_root}" . ${IMAPD_CONF} );
	while( <CYRUS_CONFIG> )
	{
		# store $_ value because subsequent operations may change it
		my( $config_key ) = $_;
		my( $config_value ) = $_;

		# strip the trailing newline from the line
		chomp($config_key);
		chomp($config_value);

		if ( ${DEBUG} ) {
			print( "- line: $config_key\n" ); }

		if ( $config_key =~ s/:.*// )
		{
			if ( $config_value =~ s/^.*:// )
			{
				# trim whitespace
				$config_value =~ s/^\s+//;
				$config_value =~ s/\s+$//;

				if ( ${DEBUG} ) {
					print( "- key: $config_key,  value: $config_value\n" ); }

				my $TAG = substr( $config_key, 0, 10 );
				if ( $TAG eq "partition-" ) {
					$TAG = substr( $config_key, 10, (length($config_key) - 10) );

					if ( ${DEBUG} ) {
						print( "- tag: $TAG,  value: $config_value\n" ); }

					if ( "$TAG" eq "default" )
					{
						my $new_default;
						if ($config_value eq "/var/spool/imap") {
							$new_default = "/Library/Server/Mail/Data/mail";
						} else {
							$new_default = "$config_value/dovecot";
						}
						qx( ${SERVER_ADMIN} settings mail:imap:partition-default = \"$new_default\" >> ${MIGRATION_LOG} );
					} else {
						qx( /bin/echo "mail:imap:partitions:_array_index:${INDEX}:path = \"${config_value}/dovecot\"" >> ${TMP_FILE} );
						qx( /bin/echo "mail:imap:partitions:_array_index:${INDEX}:partition = \"${TAG}\"" >> ${TMP_FILE} );
						$INDEX = $INDEX + 1;
					}
				} else {
					SWITCH: {
						if ( $config_key eq "imap_auth_clear" ) {
							if ( $config_value eq "no" ) {
								$clear_auth_enabled = "no";
							}
							last SWITCH;
						}
						if ( $config_key eq "imap_auth_gssapi" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:imap_auth_gssapi = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "imap_auth_cram_md5" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:imap_auth_cram_md5 = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "imap_auth_login" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:imap_auth_login = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "imap_auth_plain" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:imap_auth_plain = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "pop_auth_apop" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:pop_auth_apop = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "lmtp_over_quota_perm_failure" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:lmtp_over_quota_perm_failure = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "enable_imap" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:enable_imap = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "enable_pop" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:enable_pop = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "enable_quota_warnings" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:enable_quota_warnings = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "quota_enforce_restrictions" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:quota_enforce_restrictions = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "quotawarn" ) {
							$quota_warn_value = $config_value;
							last SWITCH;
						}
						if ( $config_key eq "log_rolling_days" ) {
							qx( ${SERVER_ADMIN} settings mail:postfix:log_rolling_days = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "log_rolling_days_enabled" ) {
							qx( ${SERVER_ADMIN} settings mail:postfix:log_rolling_days_enabled = $config_value >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "lmtp_luser_relay" ) {
							qx( ${SERVER_ADMIN} settings mail:imap:lmtp_luser_relay = $config_value >> ${MIGRATION_LOG} );
							qx( ${SERVER_ADMIN} settings mail:imap:lmtp_luser_relay_enabled = yes >> ${MIGRATION_LOG} );
							qx( ${SERVER_ADMIN} settings mail:postfix:luser_relay_enabled = yes >> ${MIGRATION_LOG} );
							last SWITCH;
						}
						if ( $config_key eq "tls_server_options" ) {
							if ( ($config_value eq "use") || ($config_value eq "require") )
							{
								if ( !($g_dovecot_ssl_key eq "") && !($g_dovecot_ssl_cert eq "") )
								{
									qx( ${SERVER_ADMIN} settings mail:imap:tls_server_options = $config_value >> ${MIGRATION_LOG} );
									qx( ${SERVER_ADMIN} settings mail:imap:tls_key_file = /etc/certificates/${g_dovecot_ssl_key} >> ${MIGRATION_LOG} );
									qx( ${SERVER_ADMIN} settings mail:imap:tls_cert_file = /etc/certificates/${g_dovecot_ssl_cert} >> ${MIGRATION_LOG} );
									qx( ${SERVER_ADMIN} settings mail:imap:tls_ca_file = /etc/certificates/${g_dovecot_ssl_ca} >> ${MIGRATION_LOG} );
								} else {
									qx( ${SERVER_ADMIN} settings mail:imap:tls_server_options = "none" >> ${MIGRATION_LOG} );
									&print_message( "Warning:", "SSL for POP/IMAP was configured with: $config_value.",
													"The migration script was unable determine SSL certificate mapping",
													"You will need to manually enable SSL for POP/IMAP" );
								}
							}
						}
						last SWITCH;
					}
				}
			}
		}
	}

	# set quota warning level
	qx( ${SERVER_ADMIN} settings mail:imap:quotawarn = $quota_warn_value >> ${MIGRATION_LOG} );

	# set clear auth 'after' all other settings to make sure PLAIN is now enabled
	#	clear requires that PLAIN be enabled in dovecot
	qx( ${SERVER_ADMIN} settings mail:imap:imap_auth_clear = $clear_auth_enabled >> ${MIGRATION_LOG} );

	# set alt mail store locations
	if ( path_exists("${TMP_FILE}") ) {
		qx( ${SERVER_ADMIN} settings < "${TMP_FILE}" >> ${MIGRATION_LOG} );
		unlink("${TMP_FILE}");
	}

	# set SSL keys for postfix
	my @smtp_use_tls = qx( ${POSTCONF} -h -c "${g_postfix_root}/private/etc/postfix" smtpd_use_tls );
	chomp(@smtp_use_tls);

	if ( $smtp_use_tls[0] eq "yes" )
	{
		if ( !($g_postfix_ssl_key eq "") && !($g_postfix_ssl_cert eq "") )
		{
			my @smtp_enforce_tls = qx( ${POSTCONF} -h -c "${g_postfix_root}/private/etc/postfix" smtpd_enforce_tls );
			chomp(@smtp_enforce_tls);
			if ( $smtp_enforce_tls[0] eq "yes" )
			{
				qx( ${SERVER_ADMIN} settings mail:postfix:tls_server_options = "require" >> ${MIGRATION_LOG} );
			} else {
				qx( ${SERVER_ADMIN} settings mail:postfix:tls_server_options = "use" >> ${MIGRATION_LOG} );
			}

			qx( ${SERVER_ADMIN} settings mail:postfix:smtpd_tls_key_file = /etc/certificates/${g_postfix_ssl_key} >> ${MIGRATION_LOG} );
			qx( ${SERVER_ADMIN} settings mail:postfix:smtpd_tls_cert_file = /etc/certificates/${g_postfix_ssl_cert} >> ${MIGRATION_LOG} );
			qx( ${SERVER_ADMIN} settings mail:postfix:smtpd_tls_CAfile = /etc/certificates/${g_postfix_ssl_ca} >> ${MIGRATION_LOG} );
		} else {
			&print_message( "Warning:", "SSL for SMTP was configured with: $smtp_use_tls[0].",
							"The migration script was unable determine SSL certificate mapping",
							"You will need to manually enable SSL for SMTP" );
			qx( ${SERVER_ADMIN} settings mail:postfix:tls_server_options = "none" >> ${MIGRATION_LOG} );
		}
	}

	close( CYRUS_CONFIG );

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_cyrus_config : E\n" ); }
} # migrate_cyrus_config


#################################################################
# migrate_dovecot_config ()

sub sedconf
{
	die unless @_ == 6;
	my $conf = shift or die;
	my $cmt = shift;
	my $key = shift or die;
	my $action = shift or die;
	my $value = shift;
	my $secpat = shift;

	my $srcpath = "$g_target_root/etc/dovecot/$conf";
	if (!open(SRC, "<", $srcpath)) {
		print LOG_FILE "can't read config file $srcpath: $!\n";
		return;
	}

	my $dstpath = "$g_target_root/etc/dovecot/$conf.new";
	if (!open(DST, ">", $dstpath)) {
		print LOG_FILE "can't create config file $dstpath: $!\n";
		close SRC;
		return;
	}

	my $cmtpat = "";
	$cmtpat = qr{(?:#\s*)?} if $cmt;

	my $done = 0;
	my $unneeded = 0;
	my @section;
	while (my $line = <SRC>) {
		chomp $line;
		$line =~ s/^(\s*)//;
		my $indent = $1;

		if ($line =~ /^([^#]+){/) {
			# begin conf section
			push @section, $1;
		} elsif ($line =~ /^}/) {
			# end conf section
			pop @section;
		} elsif ($done) {
			# skip
		} elsif (!defined($secpat) || (@section == 1 && $section[0] =~ $secpat)) {
			if ($action eq "=") {
				# replace value
				if ($line =~ s/^$cmtpat($key\s*=\s*).*$/$1$value/) {
					$done = 1;
				}
			} elsif ($action eq "+") {
				# append to list value if not already there
				if ($line =~ /$key\s*=.*(\s|=)$value(\s|$)/) {
					$unneeded = 1;
				} elsif ($line =~ s/^$cmtpat($key\s*=\s*.*)/$1 $value/) {
					$done = 1;
				}
			} elsif ($action eq "-") {
				# remove from list value
				if ($line =~ s/^$cmtpat($key\s*=\s*)$value(\s.*|$)/$1$2/ ||
				    $line =~ s/^$cmtpat($key\s*=.*)\s$value(\s.*|$)/$1$2/) {
					$done = 1;
				} elsif ($line =~ /^$cmtpat$key\s*=/) {
					$unneeded = 1;
				}
			} else {
				die;
			}
		}
		print DST "$indent$line\n";
	}

	close DST;
	close SRC;

	if (!$done) {
		if (!$unneeded) {
			print LOG_FILE "key \"$key\" not found in $srcpath, can't change value ($action $value)\n";
		}
		unlink($dstpath);
		return;
	}

	my $savedir = "$g_target_root/etc/dovecot/pre-migrate";
	mkdir($savedir, 0755);
	mkdir("$savedir/conf.d", 0755);
	my $savepath = "$savedir/$conf";
	if (!rename($srcpath, $savepath)) {
		print LOG_FILE "can't rename $srcpath -> $savepath: $!\n";
		return;
	}
	if (!rename($dstpath, $srcpath)) {
		print LOG_FILE "can't rename $dstpath -> $srcpath: $!\n";
		return;
	}
}

sub migrate_dovecot_config ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_dovecot_config : S\n" );
	}

	my ($hostname) = qx($GREP "^myhostname *=" "${g_target_root}/etc/postfix/main.cf" 2>>$MIGRATION_LOG | sed 's,.*= *,,');
	chomp $hostname;
	if (!defined($hostname) || $hostname eq "") {
		$hostname = qx(hostname);
		chomp $hostname;
	}

	my $oldtag;
	if ($g_source_version =~ /^10(\.\d+)+$/) {
		$oldtag = $g_source_version;
	} else {
		$oldtag = "old";
	}
	if (path_exists("${g_source_root}/private/etc/dovecot")) {
		qx(${RM} -rf "${g_target_root}/private/etc/dovecot/$oldtag" >> ${MIGRATION_LOG} 2>> ${MIGRATION_LOG});
		qx(${CP} -rpf "${g_source_root}/private/etc/dovecot" "${g_target_root}/private/etc/dovecot/$oldtag" >> ${MIGRATION_LOG} 2>> ${MIGRATION_LOG});
	}

	# All the config files that may change.  Others not listed won't.
	my $imap_conf =		"dovecot.conf";
	my $imap_conf_auth =	"conf.d/10-auth.conf";
	my $imap_conf_logging =	"conf.d/10-logging.conf";
	my $imap_conf_mail =	"conf.d/10-mail.conf";
	my $imap_conf_master =	"conf.d/10-master.conf";
	my $imap_conf_ssl =	"conf.d/10-ssl.conf";
	my $imap_conf_lda =	"conf.d/15-lda.conf";
	my $imap_conf_imap =	"conf.d/20-imap.conf";
	my $imap_conf_lmtp =	"conf.d/20-lmtp.conf";
	my $imap_conf_plugin =	"conf.d/90-plugin.conf";
	my $imap_conf_quota =	"conf.d/90-quota.conf";
	my $imap_conf_od =	"conf.d/auth-od.conf.ext";
	my @conf_files = ($imap_conf,
			  $imap_conf_auth,
			  $imap_conf_logging,
			  $imap_conf_mail,
			  $imap_conf_master,
			  $imap_conf_ssl,
			  $imap_conf_lda,
			  $imap_conf_imap,
			  $imap_conf_lmtp,
			  $imap_conf_plugin,
			  $imap_conf_quota,
			  $imap_conf_od);

	# All the config parameters that Server Admin or one of the mail
	# setup scripts could have changed, in the order in which they
	# appear in the config file.  "Hot" means uncommented.
	my %val = ("protocols"			=> undef,
		   "disable_plaintext_auth"	=> undef,
		   "ssl"			=> undef,
		   "ssl_cert"			=> undef,
		   "ssl_key"			=> undef,
		   "ssl_ca"			=> undef,
		   "mail_location"		=> undef,
		   "mail_debug"			=> undef,
		   "mmap_disable"		=> undef,
		   "dotlock_use_excl"		=> undef,
		   "max_mail_processes"		=> undef,
		   "aps_topic"			=> undef,
		   "postmaster_address"		=> undef,
		   "hostname"			=> undef,
		   "lda_plugins"		=> undef,
		   "auth_debug"			=> undef,
		   "auth_debug_passwords"	=> undef,
		   "auth_mechanisms"		=> undef,
		   "userdb_od_args"		=> undef,
		   "quota_warning"		=> undef,
		   "quota_warning2"		=> undef);
	my %hot;

	# determine all options set in old config file(s)
	my $from_dovecot2 = path_exists("$g_target_root/private/etc/dovecot/$oldtag/conf.d");
	for my $file (@conf_files) {
		my $dcold = "$g_target_root/private/etc/dovecot/$oldtag/$file";
		if (open(DCOLD, "<", $dcold)) {
			my @section;
			while (my $line = <DCOLD>) {
				chomp $line;
				$line =~ s/^\s+//;
				my $hot = !($line =~ s/^#\s*//);

				if ($hot && $line =~ /^([^#]+){/) {
					# begin conf section
					push @section, $1;
				} elsif ($hot && $line =~ /^}/) {
					# end conf section
					pop @section;
				} elsif ($line =~ /^(protocols)\s*=\s*(.*)/ && ($hot || !defined($hot{$1}))) {
					die unless exists $val{$1};	# check for typos
					@{$val{$1}} = split(/\s+/, $2);
					$hot{$1} = $hot;
				} elsif (@section == 0 &&
					 $line =~ /^(disable_plaintext_auth |
						     mail_location |
						     mail_debug |
						     mmap_disable |
						     dotlock_use_excl |
						     max_mail_processes |
						     auth_debug |
						     auth_debug_passwords)\s*=\s*(.*)/x &&
					 ($hot || !defined($hot{$1}))) {
					die unless exists $val{$1};	# check for typos
					$val{$1} = $2;
					$hot{$1} = $hot;
				} elsif ($line =~ /^ssl_disable\s*=\s*(.*)/ && ($hot || !defined($hot{ssl}))) {
					if ($1 eq "yes") {
						$val{ssl} = "no";
					} elsif ($hot{protocols} && grep { $_ eq "imap" || $_ eq "pop3" } @{$val{protocols}}) {
						$val{ssl} = "yes";
					} else {
						$val{ssl} = "required";
					}
					$hot{ssl} = $hot;
				} elsif ($line =~ /^(ssl_(?:cert|key|ca))_file\s*=\s*(.*)/ && ($hot || !defined($hot{$1}))) {
					die unless exists $val{$1};	# check for typos
					$val{$1} = $2;
					$hot{$1} = $hot;
				} elsif (@section == 1 && $section[0] =~ /^protocol\s+imap\s+/ &&
					 $line =~ /^(aps_topic)\s*=\s*(.*)/x &&
					 ($hot || !defined($hot{$1}))) {
					die unless exists $val{$1};	# check for typos
					$val{$1} = $2;
					$hot{$1} = $hot;
				} elsif (@section == 1 && $section[0] =~ /^protocol\s+lda\s+/ &&
					 $line =~ /^(postmaster_address |
						     hostname)\s*=\s*(.*)/x &&
					 ($hot || !defined($hot{$1}))) {
					die unless exists $val{$1};	# check for typos
					$val{$1} = $2;
					$hot{$1} = $hot;
				} elsif (@section == 1 && $section[0] =~ /^protocol\s+lda\s+/ &&
					 $line =~ /^mail_plugins\s*=\s*(.*)/ &&
					 ($hot || !defined($hot{lda_plugins}))) {
					@{$val{lda_plugins}} = split(/\s+/, $1);
					$hot{lda_plugins} = $hot;
				} elsif (@section == 1 && $section[0] =~ /^auth\s+default\s+/ &&
					 $line =~ /^mechanisms\s*=\s*(.*)/ &&
					 ($hot || !defined($hot{auth_mechanisms}))) {
					@{$val{auth_mechanisms}} = split(/\s+/, $1);
					$hot{auth_mechanisms} = $hot;
				} elsif (@section == 2 && $section[0] =~ /^auth\s+default\s+/ && $section[1] =~ /^userdb\s+od\s+/ &&
					 $line =~ /^args\s*=\s*(.*)/ &&
					 ($hot || !defined($hot{userdb_od_args}))) {
					@{$val{userdb_od_args}} = split(/\s+/, $1);
					$hot{userdb_od_args} = $hot;
				} elsif (@section == 1 && $section[0] =~ /^plugin\s+$/ &&
					 $line =~ /^(quota_warning2?)\s*=\s*(.*)/ &&
					 ($hot || !defined($hot{$1}))) {
					die unless exists $val{$1};	# check for typos
					$val{$1} = $2;
					$hot{$1} = $hot;
				}
			}
			close(DCOLD);
		} elsif ($file !~ /conf\.d/ || $from_dovecot2) {
			print LOG_FILE "can't read $dcold: $!\n";
		}
	}

	# convert options
	if ($hot{ssl_cert} && $val{ssl_cert} !~ /^</) {
		$val{ssl_cert} = "<$val{ssl_cert}";
	}
	if ($hot{ssl_key} && $val{ssl_key} !~ /^</) {
		$val{ssl_key} = "<$val{ssl_key}";
	}
	if ($hot{ssl_ca} && $val{ssl_ca} !~ /^</) {
		$val{ssl_ca} = "<$val{ssl_ca}";
	}
	if ($hot{quota_warning} && $val{quota_warning} =~ /storage=(\d+)%/) {
		$val{quota_warning} = "storage=$1%% quota-exceeded \%u";
	}
	if ($hot{quota_warning2} && $val{quota_warning2} =~ /storage=(\d+)%/) {
		$val{quota_warning2} = "storage=$1%% quota-warning \%u";
	}

	# set appropriate options in new config files
	sedconf($imap_conf,		1, "protocols",			"-", "imap",					undef)				if $hot{protocols}
		and !grep { $_ eq "imap" || $_ eq "imaps" } @{$val{protocols}};
	sedconf($imap_conf,		1, "protocols",			"-", "pop3",					undef)				if $hot{protocols}
		and !grep { $_ eq "pop3" || $_ eq "pop3s" } @{$val{protocols}};
	sedconf($imap_conf,		1, "protocols",			"+", "sieve",					undef)				if $hot{protocols}
		and grep { $_ eq "managesieve" } @{$val{protocols}};
	sedconf($imap_conf_auth,	1, "disable_plaintext_auth",	"=", $val{disable_plaintext_auth},		undef)				if $hot{disable_plaintext_auth};
	sedconf($imap_conf_ssl,		1, "ssl",			"=", $val{ssl},					undef)				if $hot{ssl};
	sedconf($imap_conf_ssl,		1, "ssl_cert",			"=", $val{ssl_cert},				undef)				if $hot{ssl_cert};
	sedconf($imap_conf_ssl,		1, "ssl_key",			"=", $val{ssl_key},				undef)				if $hot{ssl_key};
	sedconf($imap_conf_ssl,		1, "ssl_ca",			"=", $val{ssl_ca},				undef)				if $hot{ssl_ca};
	sedconf($imap_conf_mail,	0, "mail_location",		"=", $val{mail_location},			undef)				if $hot{mail_location};
	sedconf($imap_conf_logging,	1, "mail_debug",		"=", $val{mail_debug},				undef)				if $hot{mail_debug};
	sedconf($imap_conf_mail,	1, "mmap_disable",		"=", $val{mmap_disable},			undef)				if $hot{mmap_disable};
	sedconf($imap_conf_mail,	1, "dotlock_use_excl",		"=", $val{dotlock_use_excl},			undef)				if $hot{dotlock_use_excl};
	sedconf($imap_conf_master,	1, "process_limit",		"=", $val{max_mail_processes},			qr{^service\s+(imap|pop3)\s+})	if $hot{max_mail_processes};
	sedconf($imap_conf,		1, "aps_topic",			"=", $val{aps_topic},				undef)				if $hot{aps_topic};
	sedconf($imap_conf_lda,		1, "postmaster_address",	"=", $val{postmaster_address},			undef)				if $hot{postmaster_address}
		and $val{postmaster_address} !~ /example\.com/;
	sedconf($imap_conf_lda,		1, "hostname",			"=", $val{hostname},				undef)				if $hot{hostname};
	sedconf($imap_conf_imap,	1, "mail_plugins",		"+", "urlauth",					qr{^protocol\s+imap\s+})	if $hot{ssl}
		and ($val{ssl} eq "yes" || $val{ssl} eq "required");
	sedconf($imap_conf_lda,		1, "mail_plugins",		"+", "sieve",					qr{^protocol\s+lda\s+})		if $hot{lda_plugins}
		and grep { $_ eq "cmusieve" } @{$val{lda_plugins}};
	sedconf($imap_conf_lda,		1, "mail_plugins",		"+", "push_notify",				qr{^protocol\s+lda\s+})		if $hot{lda_plugins}
		and grep { $_ eq "push_notify" } @{$val{lda_plugins}};
	sedconf($imap_conf_logging,	1, "auth_debug",		"=", $val{auth_debug},				undef)				if $hot{auth_debug};
	sedconf($imap_conf_logging,	1, "auth_debug_passwords",	"=", $val{auth_debug_passwords},		undef)				if $hot{auth_debug_passwords};
	if ($hot{auth_mechanisms}) {
		sedconf($imap_conf_auth, 1, "auth_mechanisms",		"-", "cram-md5",				undef)
			if !grep { $_ eq "cram-md5" } @{$val{auth_mechanisms}};
		sedconf($imap_conf_auth, 1, "auth_mechanisms",		"+", $_,					undef)
			for @{$val{auth_mechanisms}};
	}
	sedconf($imap_conf_od,		1, "args",			"=", join(" ", @{$val{userdb_od_args}}),	qr{^userdb\s+})			if $hot{userdb_od_args};
	sedconf($imap_conf_quota,	1, "quota_warning",		"=", $val{quota_warning},			qr{^plugin\s+})			if $hot{quota_warning};
	sedconf($imap_conf_quota,	1, "quota_warning2",		"=", $val{quota_warning2},			qr{^plugin\s+})			if $hot{quota_warning2};
	qx($CP -f "$g_target_root/private/etc/dovecot/$oldtag/partition_map.conf" "$g_target_root/private/etc/dovecot/partition_map.conf");

	# Create submit.passdb with either the same password postfix is
	# configured for, or an unguessable random password.
	if (!path_exists("${g_target_root}/private/etc/dovecot/submit.passdb")) {
		my $pw;
		if (defined($hostname) && $hostname ne "" && path_exists("${g_target_root}/etc/postfix/submit.cred")) {
			($pw) = qx($GREP "^$hostname|submit|" "${g_target_root}/etc/postfix/submit.cred" 2>>$MIGRATION_LOG | sed 's,.*|,,');
			chomp $pw;
		}
		if (!defined($pw) || $pw eq "") {
			($pw) = qx(dd if=/dev/urandom bs=256 count=1 2>>$MIGRATION_LOG | env LANG=C tr -dc a-zA-Z0-9 | cut -b 1-22);
			chomp $pw;
		}
		if (defined($pw) && $pw ne "") {
			my $spnew = "${g_target_root}/private/etc/dovecot/submit.passdb";
			if (open(SPNEW, ">", $spnew)) {
				print SPNEW "submit:{PLAIN}$pw\n";
				close(SPNEW);
			} else {
				print LOG_FILE "can't write $spnew: $!\n";
			}
			qx( ${CHOWN} :mail "${g_target_root}/private/etc/dovecot/submit.passdb" >> "${MIGRATION_LOG}" 2>> "${MIGRATION_LOG}" );
			chmod(0640, "${g_target_root}/private/etc/dovecot/submit.passdb");
		}
	}

	# dovecot notify
	if ( path_exists( "${g_target_root}/private/etc/dovecot/notify" ) ) {
		qx( ${CHOWN} _dovecot:mail "${g_target_root}/private/etc/dovecot/notify" >> "${MIGRATION_LOG}" 2>> "${MIGRATION_LOG}" );
	}

	# mailusers.plist
	unlink("${g_target_root}/private/var/db/.mailusers.plist");

	# dovecot.fts.update
	mkdir("${g_target_root}/private/var/db/dovecot.fts.update");
	qx(${CHOWN} _dovecot:mail "${g_target_root}/private/var/db/dovecot.fts.update" >> "${MIGRATION_LOG}" 2>> "${MIGRATION_LOG}");
	chmod(0770, "${g_target_root}/private/var/db/dovecot.fts.update");

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_dovecot_config : E\n" ); }
} # migrate_dovecot_config


#################################################################
# migrate_postfix_spool ()

sub migrate_postfix_spool ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_postfix_spool : S\n" ); }

		# clean up sockets & pipes
		if ( path_exists( "${g_source_root}/private/var/spool/postfix/public" ) )
		{
			qx( ${RM} -rf "${g_source_root}/private/var/spool/postfix/public/"* >> ${MIGRATION_LOG} );
		}

		if ( path_exists( "${g_source_root}/private/var/spool/postfix/private" ) )
		{
			qx( ${RM} -rf "${g_source_root}/private/var/spool/postfix/private/"* >> ${MIGRATION_LOG} );
		}

		qx(${MKDIR} -p -m 755 "${g_target_root}/Library/Server/Mail/Data/spool");
		qx( rsync -av "${g_source_root}/private/var/spool/postfix/" "${g_target_root}/Library/Server/Mail/Data/spool/" >> ${MIGRATION_LOG} );

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_postfix_spool : E\n" ); }
} # migrate_postfix_spool


#################################################################
# migrate_mailman_data ()

sub migrate_mailman_data ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_mailman_data : S\n" ); }

	# only need to copy mailman data if target != source
	if ( ! (${g_target_root} eq ${g_source_root}) )
	{
		print LOG_FILE "copying mailman data: ${g_source_root}/private/var/mailman to: ${g_target_root}/private/var/mailman\n";
		qx( ${CP} -rpfv "${g_source_root}/private/var/mailman" "${g_target_root}/private/var/" >> ${MIGRATION_LOG} );
	} else {
		print LOG_FILE "not migrating mailman data, source: ${g_source_root} and target: ${g_target_root} are the same\n";
	}

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_mailman_data : E\n" ); }
} # migrate_mailman_data


#################################################################
# migrate_postfix_config ()

sub migrate_postfix_config ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_postfix_config : S\n" ); }

	# keep a copy of default 10.7 default file
	if ( path_exists( "${g_target_root}/private/etc/postfix/main.cf.default" ) ) {
		qx( ${CP} -f "${g_target_root}/private/etc/postfix/main.cf.default" "${g_target_root}/private/etc/postfix/main.cf.default.$TARGET_VER" >> ${MIGRATION_LOG} 2>> "${MIGRATION_LOG}" );
	}

	if ( path_exists( "${g_target_root}/private/etc/postfix/master.cf.default" ) ) {
		qx( ${CP} -f "${g_target_root}/private/etc/postfix/master.cf.default" "${g_target_root}/private/etc/postfix/master.cf.default.$TARGET_VER" >> ${MIGRATION_LOG} 2>> "${MIGRATION_LOG}" );
	}

	if ( path_exists("${g_source_root}/private/etc/postfix") ) {
		print LOG_FILE "copying postfix configuration: ${g_source_root}/private/etc/postfix to: ${g_target_root}/private/etc/postfix\n";
		qx( ${CP} -rpfv "${g_source_root}/private/etc/postfix" "${g_target_root}/private/etc/" >> ${MIGRATION_LOG} 2>> "${MIGRATION_LOG}" );
	}

	# update main.cf
	if (!qx($GREP "imap_submit_cred_file *=" "${g_target_root}/private/etc/postfix/main.cf" 2>>$MIGRATION_LOG)) {
		my $mcapp = "${g_target_root}/private/etc/postfix/main.cf";
		if (open(MCAPP, ">>", $mcapp)) {
			print MCAPP <<'EOT';
# (APPLE) Credentials for using URLAUTH with IMAP servers.
imap_submit_cred_file = /private/etc/postfix/submit.cred

EOT
			close(MCAPP);
		} else {
			print LOG_FILE "can't append to $mcapp: $!\n";
		}
	}
	if (!qx($GREP sacl-cache "${g_target_root}/private/etc/postfix/main.cf" 2>>$MIGRATION_LOG)) {
		my $mcapp = "${g_target_root}/private/etc/postfix/main.cf";
		if (open(MCAPP, ">>", $mcapp)) {
			print MCAPP <<'EOT';
# (APPLE) The SACL cache caches the results of Mail Service ACL lookups.
# Tune these to make the cache more responsive to changes in the SACL.
# The cache is only in memory, so bouncing the sacl-cache service clears it.
use_sacl_cache = yes
# sacl_cache_positive_expire_time = 7d
# sacl_cache_negative_expire_time = 1d
# sacl_cache_disabled_expire_time = 1m

EOT
			close(MCAPP);
		} else {
			print LOG_FILE "can't append to $mcapp: $!\n";
		}
	}

	# Create submit.cred with either the same password dovecot is
	# configured for, or an unguessable random password.
	if (!path_exists("${g_target_root}/private/etc/postfix/submit.cred")) {
		my ($hostname) = qx($GREP "^myhostname *=" "${g_target_root}/private/etc/postfix/main.cf" 2>>$MIGRATION_LOG | sed 's,.*= *,,');
		chomp $hostname;
		if (!defined($hostname) || $hostname eq "") {
			($hostname) = qx(hostname);
			chomp $hostname;
		}
		my $pw;
		if (path_exists("${g_target_root}/private/etc/dovecot/submit.passdb")) {
			($pw) = qx($GREP "^submit:" "${g_target_root}/private/etc/dovecot/submit.passdb" 2>>$MIGRATION_LOG | sed 's,.*},,');
			chomp $pw;
		}
		if (!defined($pw) || $pw eq "") {
			($pw) = qx(dd if=/dev/urandom bs=256 count=1 2>>$MIGRATION_LOG | env LANG=C tr -dc a-zA-Z0-9 | cut -b 1-22);
			chomp $pw;
		}
		if (defined($pw) && $pw ne "" && defined($hostname) && $hostname ne "") {
			my $scnew = "${g_target_root}/private/etc/postfix/submit.cred";
			if (open(SCNEW, ">", $scnew)) {
				print SCNEW "submitcred version 1\n";
				print SCNEW "$hostname|submit|$pw\n";
				close(SCNEW);
			} else {
				print LOG_FILE "can't write $scnew: $!\n";
			}
			chmod(0600, "${g_target_root}/private/etc/postfix/submit.cred");
		}
	}

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_postfix_config : E\n" ); }
} # migrate_postfix_config


#################################################################
# update_master_cf ()

sub update_master_cf ()
{
	my $has_dovecot = 0;
	my $has_greylist = 0;

	# disable virus scanning to allow for config file update
	qx( ${SERVER_ADMIN} settings mail:postfix:virus_scan_enabled = no >> ${MIGRATION_LOG} );
	qx( ${SERVER_ADMIN} settings mail:postfix:spam_scan_enabled = no >> ${MIGRATION_LOG} );

	# check to see if dovecot is already defined
	open( MASTER_CF, "<${g_target_root}" . "/private/etc/postfix/master.cf" ) or die "can't open ${g_target_root}" . "/private/etc/postfix/master.cf: $!";
	while( <MASTER_CF> )
	{
		# store $_ value because subsequent operations may change it
		my($a_line) = $_;

		# strip the trailing newline from the line
		chomp($a_line);

		if ( substr( ${a_line}, 0, 7) eq "dovecot" ) {
			$has_dovecot = 1;
		}
		if ( substr( ${a_line}, 0, 6) eq "policy" ) {
			$has_greylist = 1;
		}
	}
	close(MASTER_CF);

	if ( path_exists( "${g_target_root}" . "/private/etc/postfix/master.cf.out" ) ) {
		unlink( "${g_target_root}" . "/private/etc/postfix/master.cf.out" );
	}

	my $tlsmgr = 0;
	my $skip_line = 0;
	my $skip_comment = 0;
	my $update_deliver = 0;

	open( MASTER_CF, "<${g_target_root}" . "/private/etc/postfix/master.cf" ) or die "can't open ${g_target_root}" . "/private/etc/postfix/master.cf: $!";
	open (MASTER_CF_OUT, ">${g_target_root}" . "/private/etc/postfix/master.cf.out" ) or die "can't open ${g_target_root}" . "/private/etc/postfix/master.cf.out: $!";
	while( <MASTER_CF> )
	{
		# store $_ value because subsequent operations may change it
		my($a_line) = $_;

		# strip the trailing newline from the line
		chomp($a_line);

		if ( (substr( ${a_line}, 0, 6) eq "tlsmgr") || (substr( ${a_line}, 0, 7) eq "#tlsmgr") )
		{
			if ( $tlsmgr == 0 )
			{
				if ( substr( ${a_line}, 0, 7) eq "#tlsmgr" )
				{
					print MASTER_CF_OUT "${a_line}";
					print MASTER_CF_OUT "\n";
				}
				print MASTER_CF_OUT "tlsmgr    unix  -       -       n       1000?   1       tlsmgr";
				print MASTER_CF_OUT "\n";
				$tlsmgr = 1;
			}
		}
		elsif (substr( ${a_line}, 0, 32) eq "# === End auto-generated section")
		{
			print MASTER_CF_OUT "#submission inet  n       -       n       -       -       smtpd" . "\n";
			print MASTER_CF_OUT "#  -o smtpd_tls_security_level=encrypt" . "\n";

			print MASTER_CF_OUT "${a_line}";
			print MASTER_CF_OUT "\n";
		}
		elsif (substr( ${a_line}, 0, 10) eq "submission")
		{
			# skip enabled submission settings
			$skip_line = 1;
		}
		elsif ( $skip_line == 1 )
		{
			if ( (substr( ${a_line}, 0, 1) eq " ") || (substr( ${a_line}, 0, 1) eq "	") ) {
				# keep skipping until we hit non-whitespace line
			} else {
				$skip_line = 0;
				print MASTER_CF_OUT "${a_line}";
				print MASTER_CF_OUT "\n";
			}
		}
		elsif (substr( ${a_line}, 0, 11) eq "#submission")
		{
			# skip enabled submission settings
			$skip_comment = 1;
		}
		elsif ( $skip_comment == 1 )
		{
			if ( (substr( ${a_line}, 0, 2) eq "# ") || (substr( ${a_line}, 0, 2) eq "#	") ) {
				# keep skipping until we hit non-whitespace line
			} else {
				$skip_comment = 0;
				print MASTER_CF_OUT "${a_line}";
				print MASTER_CF_OUT "\n";
			}
		}
		elsif (substr($a_line, 0, 7) eq "dovecot")
		{
			# update deliver to dovecot-lda
			$update_deliver = 1;
			print MASTER_CF_OUT "${a_line}";
			print MASTER_CF_OUT "\n";
		}
		elsif ( $update_deliver == 1 )
		{
			# skip comments
			my $line = $a_line;
			$line =~ s/^\s+//;
			if (index($line, "#") == 0) {
				print MASTER_CF_OUT "$a_line\n";
				next;
			}

			# remove -n and/or -s options
			$line = $a_line;
			$line =~ s/-[ns] //g;
			$line =~ s/[ \t]-[ns]//g;

			# skip valid settins that are not the deliver path
			if ((index($line, " ") == 0 || index($line, "\t") == 0) && (index($line, "/dovecot/deliver") == -1)) {
				print MASTER_CF_OUT "$line\n";
				next;
			}

			# this is the line we care about
			if ((index($line, "/dovecot/deliver")) > 0) {
				# change deliver to dovecot-lda
				$line =~ s/deliver/dovecot-lda/;
		
				print MASTER_CF_OUT "$line\n";
				next;
			}

			print MASTER_CF_OUT "$line\n";
			$update_deliver = 0;
		}
		elsif ( !("${a_line}" eq "") )
		{
			print MASTER_CF_OUT "${a_line}";
			print MASTER_CF_OUT "\n";
		}
	}

	# add dovecot 
	if ( $has_dovecot == 0 )
	{
		print MASTER_CF_OUT "#" . "\n";
		print MASTER_CF_OUT "# Dovecot" . "\n";
		print MASTER_CF_OUT "#" . "\n";
		print MASTER_CF_OUT "dovecot   unix  -       n       n       -       25      pipe" . "\n";
		print MASTER_CF_OUT "  flags=DRhu user=_dovecot:mail argv=/usr/libexec/dovecot/dovecot-lda -d \${user}" . "\n";
	}

	# add greylist policy
	if ( $has_dovecot == 0 )
	{
		print MASTER_CF_OUT "#" . "\n";
		print MASTER_CF_OUT "# Greylist policy server" . "\n";
		print MASTER_CF_OUT "#" . "\n";
		print MASTER_CF_OUT "policy    unix  -       n       n       -       -       spawn" . "\n";
		print MASTER_CF_OUT "  user=nobody:mail argv=/usr/bin/perl /usr/libexec/postfix/greylist.pl" . "\n";
	}

	close( MASTER_CF );
	close( MASTER_CF_OUT );

	unlink("${g_target_root}" . "/private/etc/postfix/master.cf");
	move( "${g_target_root}" . "/private/etc/postfix/master.cf.out", "${g_target_root}" . "/private/etc/postfix/master.cf");

} # update_master_cf


#################################################################
# migrate_cyrus_data ()

sub migrate_cyrus_data ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_cyrus_data : S\n" ); }

	if ( do_cyrus_config_check( "configuration" ) ) {
		return;
	}
	do_xsan_check();
	do_cyrus_db_cleanup();
	do_cyrus_dovecot_migration();

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_cyrus_data : E\n" ); }
} # migrate_cyrus_data


#################################################################
# migrate_dovecot_data ()

sub migrate_dovecot_data ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::migrate_dovecot_data : S\n" ); }

	# migrate sieve scripts
	if ( path_exists( "${g_source_root}/private/var/spool/imap/dovecot/sieve-scripts" ) )
	{
		if ( ! path_exists( "${g_target_root}/Library/Server/Mail/Data/rules" ) )
		{
			qx( mkdir -p "${g_target_root}/Library/Server/Mail/Data/rules" >> "${MIGRATION_LOG}" );
		}

		qx( rsync -av "${g_source_root}/private/var/spool/imap/dovecot/sieve-scripts/" "${g_target_root}/Library/Server/Mail/Data/rules/" >> "${MIGRATION_LOG}" );
	}

	# get default path
	open( PARTITIONS, "<${g_source_root}" . "/private/etc/dovecot/partition_map.conf" );
	while( <PARTITIONS> )
	{
		# store $_ value because subsequent operations may change it
		my( $a_line ) = $_;

		# strip the trailing newline from the line
		chomp( $a_line );

		my $offset = index($a_line, ":");
		if ( $offset != -1 )
		{
			# strip the quotes
			my $a_path = substr( $a_line, $offset + 1 );

			# is it local
			if ( (substr($a_path, 0, 5) eq "/var/") || (substr($a_path, 0, 5) eq "/etc/") )
			{
				if ( path_exists( "${g_source_root}/private" . $a_path) )
				{
					qx( mkdir -p "${g_target_root}/private${a_path}" >> "${MIGRATION_LOG}" );
					qx( ${CP} -rpfv "${g_source_root}/private${a_path}/"* "${g_target_root}/private${a_path}" >> "${MIGRATION_LOG}" );

					push( @g_clean_partitions, "${g_source_root}/private" . $a_path );
				}
			} elsif ($a_path =~ m,^/Library/Server/Mail/Data/,) {
				qx( mkdir -p "${g_target_root}${a_path}" >> "${MIGRATION_LOG}" );
				qx( ${CP} -rpfv "${g_source_root}${a_path}/"* "${g_target_root}${a_path}" >> "${MIGRATION_LOG}" );

				push( @g_clean_partitions, "${g_source_root}" . $a_path );
			}
			qx( ${CHOWN} -R _dovecot:mail "${g_target_root}${a_path}" >> "${MIGRATION_LOG}" );
		}
	}
	close( PARTITIONS );

	# set ownership on default dovecot mail data store & sive scripts
	qx( ${CHOWN} -R _dovecot:mail "${g_target_root}/Library/Server/Mail/Data/mail" >> "${MIGRATION_LOG}" );
	qx( ${CHOWN} -R _dovecot:mail "${g_target_root}/Library/Server/Mail/Data/rules" >> "${MIGRATION_LOG}" );

	if ( ${FUNC_LOG} ) {
		printf( "::migrate_dovecot_data : E\n" ); }
} # migrate_dovecot_data

#################################################################
# escape_str ()
#	only a-z, A-Z, 0-9 and % allowed
#	and all other characters are hex-encoded

sub escape_str
{
	my $s = shift;
	$s =~ s/([^a-zA-Z0-9])/sprintf("%%%02x", ord($1))/eg;
	return $s;
} # escape_str

#################################################################
# scan_mail_acct ()
#	scan mail account for sub mailboxes and create indexes for each

sub scan_mail_acct
{
	my $in_dir = $_[0];
	my $clean_name = escape_str($_[1]);
	my $dst_path = $g_target_root . "/var/db/dovecot.fts.update";

	if (!opendir(MAIL_ACCT_DIR, $in_dir)) {
		print LOG_FILE "cannot open mailbox: $in_dir\n";
		return;
	}

	my @mailboxes = readdir(MAIL_ACCT_DIR);
	closedir(MAIL_ACCT_DIR);

	# create index for INBOX
	if(open(MY_FILE, ">$dst_path/" . $clean_name . ".INBOX")) {
		print MY_FILE "\n";
		close(MY_FILE);
	}

	$in_dir .= "/";

	# create index for any sub-mailbox
	my $file = "";
	foreach $file (@mailboxes) {
		my $a_path = $in_dir.$file;
		if (-d $a_path) {
			if (($file ne ".") && ($file ne "..")) { 
				if (substr($file, 0, 1) eq ".") {
					$file = substr($file, 1, length($file) -1);
					if (open(MY_FILE, ">$dst_path/$clean_name." . escape_str($file))) {
						print MY_FILE "\n";
						close(MY_FILE);
					}
				}
			}
		}
	}
} # scan_mail_acct

#################################################################
# create_fts_indexes ()
#	create fts indexes for all mail accounts

sub create_fts_indexes ()
{
	if ($FUNC_LOG){
		printf("::create_fts_indexes : S\n");}

	# create indexes for accounts on all partitions
	open(PARTITIONS, "<$g_target_root" . "/private/etc/dovecot/partition_map.conf");
	while(<PARTITIONS>) {
		my($a_line) = $_;
		chomp($a_line);

		# get partition path
		my $offset = index($a_line, ":");
		if ($offset != -1) {
			# strip the quotes
			my $a_path = substr($a_line, $offset + 1);

			# get list of user mail account
			if (!opendir(MAIL_DIRS, $a_path)) {
				print LOG_FILE "cannot open: $a_path\n";
				next;
			}
			my @acct_dirs= readdir(MAIL_DIRS);
			closedir(MAIL_DIRS);

			# make the fts indexes for valid user accounts
			my $file;
			foreach $file (@acct_dirs) {
				next unless $file =~ /^[A-F0-9-]+$/;
				if(($file ne ".") && ($file ne "..")) { 
					# convert GUID to valid user ID
					my $user_id = qx($CVT_MAIL_DATA -u $file);
					if (substr($user_id, 0, 16) ne "No user id found") {
						chomp($user_id);
						scan_mail_acct($a_path . "/" . $file, $user_id);
					}
				}
			}
		}
	}
	close(PARTITIONS);

	if ($FUNC_LOG){
		printf("::create_fts_indexes : E\n");}
} # create_fts_indexes

################################################################################
# We only want to run this script if the previous system version is greater
#  than or equal to 10.5 and less than 10.7!

sub is_valid_version () 
{
    if (${FUNC_LOG}) {
		print( "::is_valid_version : S\n"); }

	my ( $valid ) = 0;

	if ( (substr(${g_source_version}, 0, 4) >= ${MIN_VER}) && (substr(${g_source_version}, 0, 4) < ${MAX_VER}) )
	{
		$valid = 1;
    	if (${DEBUG}) {
			printf( "- valid: ${g_source_version}\n");}

		if ( substr(${g_source_version}, 0, 4) eq "10.6" or substr(${g_source_version}, 0, 4) eq "10.7" )
		{
			$g_sl_src = 1;
		}
	} else {
		printf( "- Version supplied was not valid: %s\n", $g_source_version );
	}

    if (${FUNC_LOG}) {
		print( "::is_valid_version : E\n"); }

	return( ${valid} );
} # is_valid_version


################################################################################
# verify the language setting

sub is_valid_language () 
{
    if (${FUNC_LOG}) {
		print( "::is_valid_language : S\n"); }

	my ( $valid ) = 0;
    my ( $lang ) = $g_language;

	if ( (${lang} eq "en") || (${lang} eq "fr") || (${lang} eq "de") || (${lang} eq "ja") )
	{
		$valid = 1;
    	if (${DEBUG}) {printf( "- valid: ${lang}\n");}
	}

    if (${FUNC_LOG}) {
		print( "::is_valid_language : E\n"); }

	return( ${valid} );
} # is_valid_language


################################################################################
# parse_options()
#	Takes a list of possible options and a boolean indicating
#	whether the option has a value following, and sets up an associative array
#	%opt of the values of the options given on the command line. It removes all
#	the arguments it uses from @ARGV and returns them in @optArgs.

sub parse_options
{
    my (@optval) = @_;
    my ($opt, @opts, %valFollows, @newargs);

    while (@optval)
	{
		$opt = shift(@optval);
		push(@opts,$opt);
		$valFollows{$opt} = shift(@optval);
    }

    my @optArgs = ();
    my %opt = ();

	my $arg;
    arg: while (defined($arg = shift(@ARGV)))
	{
		foreach my $opt (@opts)
		{
			if ($arg eq $opt)
			{
				push(@optArgs, $arg);
				if ($valFollows{$opt})
				{
					$opt{$opt} = shift(@ARGV);
					push(@optArgs, $opt{$opt});
				} else {
					$opt{$opt} = 1;
				}
				next arg;
			}
		}
		push(@newargs,$arg);
    }
    @ARGV = @newargs;
} # parse_options


################################################################################
# print script argument keys and values

sub print_script_args ()
{
	my %script_args = @_;
	while(my ($theKey, $theVal) = each (%script_args)) {
		print "$theKey: $theVal\n";
	}
} # print_script_args


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

	if (-e "${in_path}")
	{
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
# check if a path's parent exists

sub parent_exists () 
{
    if (${FUNC_LOG}) {
		print( "::parent_exists : S\n");  }

	my ($out_val) = 0;
	my ($in_path) = @_;

	my $parent_path = qx( /usr/bin/dirname "${in_path}" );
	chomp $parent_path;

   	if (${DEBUG}) {
		printf( "- path: %s\n", "${in_path}"); }

	if ( -e "${parent_path}" )
	{
		$out_val = 1;
    	if ( ${DEBUG} ) {
			printf( "-- Exists\n"); }
	} else {
    	if ( ${DEBUG} ) {
			printf( "-- Does not exist\n"); }
	}

    if (${FUNC_LOG}) {
		print( "::parent_exists : E\n"); }

	return( $out_val );
} # parent_exists


################################################################################
# Create the parent directory

sub create_parent_dir ()
{
    if ( ${FUNC_LOG} ) {
		print( "::create_parent_dir : S\n"); }

	my ($out_val) = 0;
	my ($in_path) = @_;

	my $parent_dir = qx(/usr/bin/dirname "${in_path}");
	chomp($parent_dir);

   	if (${DEBUG}) {
		printf( "- parent_dir: %s\n", "${parent_dir}"); }

	qx( /bin/mkdir -p "${parent_dir}" >> "${MIGRATION_LOG}" );

    if (${FUNC_LOG}) {
		print("::create_parent_dir : E\n"); }

	if ( -e "${parent_dir}" ) {
		$out_val = 1; }

	return( ${out_val} );

} # create_parent_dir


################################################################################
# check if a path exists

sub do_cleanup ($)
{
    if (${FUNC_LOG}) {
		print( "::do_cleanup : S\n"); }

	my ($in_path) = @_;

	if ( path_exists("${in_path}") )
	{
		print LOG_FILE "Removing source: ${in_path}\n";
		qx( ${RM} -rf "${in_path}" 2>&1 >> ${MIGRATION_LOG} );
	}

	if ( ${FUNC_LOG} ) {
		printf( "::do_cleanup : S\n" ); }
} # do_cleanup


################################################################################
# Show usage

sub Usage()
{
	print("--purge <0 | 1>   \"1\" means remove any files from the old system after you've migrated them, \"0\" means leave them alone." . "\n");
	print("--sourceRoot <path> The path to the root of the system to migrate" . "\n");
	print("--sourceType <System | TimeMachine> Gives the type of the migration source, whether it's a runnable system or a " . "\n");
	print("                  Time Machine backup." . "\n");
	print("--sourceVersion <ver> The version number of the old system (like 10.5.7 or 10.6). Since we support migration from 10.5, 10.6," . "\n");
	print("                  and other 10.7 installs." . "\n");
	print("--targetRoot <path> The path to the root of the new system." . "\n");
	print("--language <lang> A language identifier, such as \"en.\" Long running scripts should return a description of what they're doing " . "\n");
	print("                  (\"Migrating Open Directory users\"), and possibly provide status update messages along the way. These messages " . "\n");
	print("                  need to be localized (which is not necessarily the server running the migration script). " . "\n");
	print("                  This argument will identify the Server Assistant language. As an alternative to doing localization yourselves " . "\n");
	print("                  send them in English, but in case the script will do this it will need this identifier." . "\n");
	print(" " . "\n");
	exit( 1 );
} # Usage

################################################################################
# do_migration()

sub do_migration()
{
	my %script_args = @_;
	my $state = "SERVICE_DISABLE";
	my $running_state = "STOPPED";

	##  Set the globals with the user options
	$g_purge = $script_args{"--purge"};
	$g_source_root = $script_args{"--sourceRoot"};
	$g_source_type = $script_args{"--sourceType"};
	$g_source_version = $script_args{"--sourceVersion"};
	$g_target_root = $script_args{"--targetRoot"};
	$g_language = $script_args{"--language"};

	## log migration start time
	my $start_time = localtime();
	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Begin Mail Migration: $start_time" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";

	##  log settings
	print LOG_FILE "purge: ${g_purge}\n";
	print LOG_FILE "sourceRoot: ${g_source_root}\n";
	print LOG_FILE "sourceType: ${g_source_type}\n";
	print LOG_FILE "sourceVersion: ${g_source_version}\n";
	print LOG_FILE "targetRoot: ${g_target_root}\n";
	print LOG_FILE "language: ${g_language}\n";

	##  verify source volume
	if ( ! path_exists( "${g_source_root}" ) ) {
		print LOG_FILE "Source for upgrade/migration: ${g_source_root} does not exist.\n";
		print( "Source for upgrade/migration: ${g_source_root} does not exist.\n" );
		Usage();
	}

	##  verify destination volume
	if ( ! path_exists("${g_target_root}") ) {
		print LOG_FILE "Destination for upgrade/migration: ${g_target_root} does not exist.\n";
		print( "Destination for upgrade/migration: ${g_target_root} does not exist.\n" );
		Usage();
	}

	## temp check ##
	if ( "${g_source_type}" eq "TimeMachine" ) {
		print LOG_FILE "Migration from Time Machine backups are not supported at this time.\n";
		print( "Migration from Time Machine backups are not supported at this time\n" );
		exit( 1 );
	}

	## temp check ##
	if ( !("${g_target_root}" eq "/") ) {
		print LOG_FILE "Migration to non-boot volumes are not supported at this time.\n";
		print( "Migration from Time Machine backups are not supported at this time\n" );
		exit( 1 );
	}

	##  verify language setting
	if ( ! is_valid_language() ) {
		print LOG_FILE "Did not supply a valid language for the --language parameter, needs to be one of [en|fr|de|ja]\n";
		print( "Did not supply a valid language for the --language parameter, needs to be one of [en|fr|de|ja]\n" );
		Usage();
	}

	##  check version info
	if ( ! is_valid_version() ) {
		print( "Did not supply a valid version for the --sourceVersion parameter, needs to be >= ${MIN_VER} and < ${MAX_VER}\n" );
		Usage();
	}

	if ( !("${g_source_root}" eq "/Previous System") ) {
		$g_postfix_root = $g_source_root;
		$g_source_uuid = uuidof($g_source_root);
	}

	## set migration plist info
	if ( path_exists(${g_migration_plist}) ) {
		qx( ${RM} -f ${g_migration_plist} );
	}

	## create migration plist
	qx( ${PLIST_BUDDY} -c 'Add :purge integer ${g_purge}' ${g_migration_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :sourceVersion string ${g_source_version}' ${g_migration_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :sourceRoot string ${g_source_root}' ${g_migration_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :targetRoot string ${g_target_root}' ${g_migration_plist} );

	qx( ${PLIST_BUDDY} -c 'Add :sourceUUID string ${g_source_uuid}' ${g_migration_plist} )
		if $g_source_uuid ne "";

	# enable migration launchd plist
	qx( ${PLIST_BUDDY} -c 'Set :Disabled bool false' ${g_migration_ld_plist} );

	## reset local users migration flag
	qx( ${CVT_MAIL_DATA} -k reset );

	## get mail services state
	get_services_state();

	## stop any running mail services
	halt_mail_services();

	## Parse ServerVersion passed in.
	get_server_version( $g_source_version );

	######################
	# General settings
	######################

	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Migrate General Settings" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";

	# get ssl certificate settings
	get_ssl_certs();

	# migrate general settings plist if (src != dst)
	if ( !("${g_target_root}" eq "${g_source_root}") ) {
		if ( path_exists("${g_source_root}/private/etc/MailServicesOther.plist") ) {
			qx( ${CP} "${g_source_root}/private/etc/MailServicesOther.plist" "${g_target_root}/private/etc/" ); }
		if ( path_exists("${g_source_root}/private/etc/mail") ) {
			qx( ${CP} -rpf "${g_source_root}/private/etc/mail" "${g_target_root}/private/etc/" ); }

		my $other_settings = "${g_source_root}/private/etc/MailServicesOther.plist";
		my $other_dict = NSDictionary->dictionaryWithContentsOfFile_( ${other_settings} );
		if ( $other_dict && $$other_dict ) {
			my $imap_dict = $other_dict->objectForKey_( "imap" );
			if ( !$imap_dict || !$$imap_dict ) {
				my $other_path = "/private/etc/MailServicesOther.plist";
				my $other_path_str = NSString->stringWithCString_($other_path);
				my $my_imap_dict = NSMutableDictionary->alloc->init;

				$other_dict->setObject_forKey_($my_imap_dict, "imap");
				$other_dict->writeToFile_atomically_( $other_path_str, 0 );
			}
		}

		# set notification service
		my $notify_state = qx( ${PLIST_BUDDY} -c 'Print :imap:notification_server_enabled' "${g_target_root}/private/etc/MailServicesOther.plist" );
		chomp(${notify_state});
		if ( ${notify_state} eq "true" ) {
			qx( ${SERVER_ADMIN} settings mail:imap:notification_server_enabled = yes >> ${MIGRATION_LOG} );
		}

		# get current service state
		$running_state = qx( ${PLIST_BUDDY} -c 'Print :service_state' "${g_target_root}/private/etc/MailServicesOther.plist" );
		chomp( ${running_state} );

		# set migration state
		$state = qx( ${PLIST_BUDDY} -c 'Print :state' "${g_target_root}/private/etc/MailServicesOther.plist" );
		chomp( ${state} );
		qx( ${PLIST_BUDDY} -c 'Add :serviceState string ${state}' ${g_migration_plist} );
	}

	if ( !path_exists("/private/etc/imapd.conf") ) {
		if ( path_exists("/private/etc/imapd.conf.default") ) {
			qx( ${CP} "/private/etc/imapd.conf.default" "/private/etc/imapd.conf" );
		}
	}

	# set webmail state
	my @webmail_state = qx(/usr/sbin/webappctl status com.apple.webapp.webmailserver);
	chomp(@webmail_state);
	if ( $webmail_state[0] eq "web:state = \"RUNNING\"" ) {
		qx( ${SERVER_ADMIN} settings mail:imap:request_enable_webmail = yes >> ${MIGRATION_LOG} );
	} else {
		qx( ${SERVER_ADMIN} settings mail:imap:request_enable_webmail = no >> ${MIGRATION_LOG} );
	}

	# migrate log level settings
	migrate_log_settings();

	######################
	# mailman
	######################

	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Migrate Mailing List Settings and Data" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";

	# migrate mailman data
	if ( !("${g_target_root}" eq "${g_source_root}") ) {
		migrate_mailman_data();
	}

	# get mailman enabled settigs
	my $mailman_enabled;

	# get mailman dictionary from general settings
	my $other_settings = "${g_source_root}/private/etc/MailServicesOther.plist";
	my $other_dict = NSDictionary->dictionaryWithContentsOfFile_( ${other_settings} );
	if ( $other_dict && $$other_dict ) {
		my $mailman_dict = $other_dict->objectForKey_( "mailman");
		if ( $mailman_dict && $$mailman_dict ) {
			if ( $mailman_dict->isKindOfClass_( NSDictionary->class ) ) {
				my $mailman_enabled_key = $mailman_dict->objectForKey_( "mailman_enabled");
				if ( $mailman_enabled_key && $$mailman_enabled_key) {
					$mailman_enabled = obj_value( $mailman_enabled_key );
				}
			}
		}
	}

	if ( ${mailman_enabled} ) {
		qx( ${SERVER_ADMIN} settings mail:mailman:enable_mailman = yes >> ${MIGRATION_LOG} );
	}

	# clean up mailman data
	qx( /usr/share/mailman/bin/check_perms -f >> "${MIGRATION_LOG}" );

	######################
	# amavisd
	######################

	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Migrate Junkmail & Virus Settings and Data" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";

	if ( "${g_target_root}" eq "/" ) {
		# if boot vol, save spam & virus settings to re-enable later
		open( AMAVIS_CONF, "<${g_source_root}" . "/private/etc/amavisd.conf" );
		while( <AMAVIS_CONF> )
		{
			# store $_ value because subsequent operations may change it
			my( $a_line ) = $_;

			# strip the trailing newline from the line
			chomp($a_line);

			# looking for: #@bypass_spam_checks_maps  = (1);
			my $a_key = index( ${a_line}, "\@bypass_spam_checks_maps" );
			my $a_val = index( ${a_line}, "=" );
			if ( ($a_key != -1) && ($a_val != -1) )
			{
				if ( substr( ${a_line}, 0, 1) eq "#" )
				{
					print LOG_FILE "Junk mail scanning enabled in: ${g_source_root} \n";
					$g_enable_spam = 1;
				}
			}

			# looking for: #@bypass_virus_checks_maps = (1);
			$a_key = index( ${a_line}, "\@bypass_virus_checks_maps" );
			$a_val = index( ${a_line}, "=" );
			if ( ($a_key != -1) && ($a_val != -1) )
			{
				if ( substr( ${a_line}, 0, 1) eq "#" )
				{
					print LOG_FILE "Virus scanning enabled in: ${g_source_root} \n";
					$g_enable_virus = 1;
				}
			}

		}
		close(AMAVIS_CONF);
	} else {
		my $src_file = "${g_source_root}/private/etc/MailServicesOther.plist";
		my $src_dict = NSDictionary->dictionaryWithContentsOfFile_( ${src_file} );
		if ( $src_dict && $$src_dict )
		{
			my $postfix_dict = $src_dict->objectForKey_( "postfix");
			if ( $postfix_dict && $$postfix_dict)
			{
				if ( $postfix_dict->isKindOfClass_( NSDictionary->class ) )
				{
					$g_enable_spam = obj_value( $postfix_dict->objectForKey_( "spam_enabled") );
				}
			}
		}
	}

	# save old amavisd.conf file
	if ( path_exists("${g_target_root}/private/etc/amavisd.conf") ) {
		qx( ${CP} -v "${g_target_root}/private/etc/amavisd.conf" "${g_target_root}/private/etc/amavisd.conf.default" >> "${MIGRATION_LOG}" );
	}
	if ( path_exists("${g_source_root}/private/etc/amavisd.conf") ) {
		qx( ${CP} -v "${g_source_root}/private/etc/amavisd.conf" "${g_target_root}/private/etc/amavisd.conf" >> "${MIGRATION_LOG}" );
	}

	# copy amavis var directory
	if ( !("${g_target_root}" eq "${g_source_root}") )
	{
		if ( path_exists("${g_source_root}/private/var/amavis/amavisd.sock") ) {
			qx( ${RM} -f "${g_source_root}/private/var/amavis/amavisd.sock" >> "${MIGRATION_LOG}" );
		}

		if ( path_exists("${g_source_root}/private/var/amavis") ) {
			qx( ${CP} -rfv "${g_source_root}/private/var/amavis" "${g_target_root}/private/var/" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
		}
		qx( ${CHOWN} -R _amavisd:_amavisd "${g_target_root}/private/var/amavis" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
	}

	######################
	# clamav
	######################

	# Clam AV config will be migrated by 66_clamav_migrator
	#	in /System/Library/ServerSetup/MigrationExtras

	# migrate database upgrade check times
	migrate_db_update_times();

	######################
	# postfix
	######################

	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Migrate SMTP Settings and Data" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";

	# if not upgrading same volume
	if ( !("${g_target_root}" eq "${g_source_root}") ) {
		# migrate postfix config settings
		migrate_postfix_config();
	}

	# upgrade master.cf settings
	update_master_cf();

	# set keys in main.cf
	qx( ${POSTCONF} -e mail_owner=_postfix >> "${MIGRATION_LOG}" );
	qx( ${POSTCONF} -e setgid_group=_postdrop >> "${MIGRATION_LOG}" );
	qx( ${POSTCONF} -e mailbox_transport=dovecot >> "${MIGRATION_LOG}" );

	# enable IPv4 & IPv6
	my @inet_protocols = qx( $POSTCONF -h -c "$g_postfix_root/private/etc/postfix" inet_protocols );
	chomp(@inet_protocols);
	# check for custom settings and only change if current settings are IPv4 or misconfigured
	if ( (@inet_protocols > 0) && (($inet_protocols[0] eq "ipv4") || ($inet_protocols[0] eq "")) ) {
		qx( $POSTCONF -e inet_protocols=all >> "${MIGRATION_LOG}" );
	}

	my $virt_mb = qx( ${POSTCONF} -h -c "${g_postfix_root}/private/etc/postfix" virtual_mailbox_domains );
	chomp($virt_mb);
	my $virt_trans = qx( ${POSTCONF} -h -c "${g_postfix_root}/private/etc/postfix" virtual_transport );
	chomp($virt_trans);
	if ( (index($virt_mb, "hash:/etc/postfix/virtual_domains") != -1) && (index($virt_trans, "lmtp:unix:") != -1) )
	{
		qx( ${SERVER_ADMIN} settings mail:postfix:enable_virtual_domains = yes >> ${MIGRATION_LOG} );
		qx( ${POSTCONF} -e virtual_transport=virtual >> "${MIGRATION_LOG}" );
		qx( ${POSTCONF} -e virtual_mailbox_domains=$virt_mb >> "${MIGRATION_LOG}" );
	} else {
		qx( ${SERVER_ADMIN} settings mail:postfix:enable_virtual_domains = no >> ${MIGRATION_LOG} );
	}

	######################
	# cyrus to dovecot
	######################

	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Migrate Mail Settings and Data" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";

	## setup config filse
	qx( "/System/Library/ServerSetup/CleanInstallExtras/SetupDovecot.sh" >> "${MIGRATION_LOG}" );

	if ( $g_sl_src )
	{
		# copy dovecot config settings
		migrate_dovecot_config();

		# copy dovecot mail data
		migrate_dovecot_data();

		# creat fts indexes
		create_fts_indexes();
	} else {
		# if dest vol != boot vol, then do not migrate config settings
		if ( "${g_target_root}" eq "/" ) {
			# move cyrus config settings to dovecot
			migrate_cyrus_config();
		} else {
			&print_message( "Warning:", "Configuration migration is only supported for boot volume.", "No configuration data was migrated." );
		}

		# migrate cyrus mail data
		migrate_cyrus_data();
	}

	# setup migration launchd plist
	qx( ${PLIST_BUDDY} -c 'Add :Disabled bool true' ${g_migration_ld_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :RunAtLoad bool true' ${g_migration_ld_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :Label string com.apple.mail_migration' ${g_migration_ld_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :Program string /usr/libexec/dovecot/mail_data_migrator.pl' ${g_migration_ld_plist} );
	qx( ${PLIST_BUDDY} -c 'Add :ProgramArguments string /usr/libexec/dovecot/mail_data_migrator.pl' ${g_migration_ld_plist} );

	######################
	# enable settings
	######################

	# only on boot vol
	if ( "${g_target_root}" eq "/" )
	{
		# update master.cf
		print LOG_FILE "Updating /etc/postfix/master.cf\n";
		qx( ${SERVER_ADMIN} command mail:command = validateMasterCf >> ${MIGRATION_LOG} );

		qx( /usr/sbin/postfix upgrade-configuration >> "${MIGRATION_LOG}" );
		qx( /usr/sbin/postfix check >> "${MIGRATION_LOG}" );
		qx( /usr/sbin/postfix set-permissions >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );

		if ( $g_enable_spam == 1 ) {
			qx( ${SERVER_ADMIN} settings mail:postfix:smtp_uce_controlls = 1 >> ${MIGRATION_LOG} );
			qx( ${SERVER_ADMIN} settings mail:postfix:spam_scan_enabled = yes >> ${MIGRATION_LOG} );
		}

		if ( $g_enable_virus == 1 ) {
			qx( ${SERVER_ADMIN} settings mail:postfix:virus_scan_enabled = yes >> ${MIGRATION_LOG} );
		}
	}

	qx( /usr/sbin/postconf -e mail_owner=_postfix >> ${MIGRATION_LOG} );
	qx( /usr/sbin/postconf -e setgid_group=_postdrop >> ${MIGRATION_LOG} );

	######################
	# clean up
	######################

	if ( ($g_purge == 1) && !("${g_target_root}" eq "${g_source_root}") ) {
		do_cleanup( "${g_source_root}/private/etc/imapd.conf" );
		do_cleanup( "${g_source_root}/private/etc/cyrus.conf" );
		do_cleanup( "${g_source_root}/private/var/mailman" );
		do_cleanup( "${g_source_root}/private/var/spool/postfix" );
		do_cleanup( "${g_source_root}/private/etc/postfix" );
		do_cleanup( "${g_source_root}/private/var/amavis" );
		do_cleanup( "${g_source_root}/private/etc/MailServicesOther.plist" );
		do_cleanup( "${g_source_root}/private/etc/amavisd.conf" );
	}

	# start mail service after migration is complete
	if ( ${running_state} eq "RUNNING" ) {
		print LOG_FILE "Starting mail services\n";
		qx( ${SERVER_ADMIN} start mail >> ${MIGRATION_LOG} );
	}

	my $end_time = localtime();
	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Mail Migration Complete: $end_time" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";
} # do_migration

sub uuidof
{
	my $volume = shift;

	my $uuid = "";
	if (defined($volume) && $volume ne "" && -e $volume) {
		my @infos = qx(/usr/sbin/diskutil info "$volume");
		for (@infos) {
			if (/\s*Volume UUID:\s*([0-9A-F]{8}(-[0-9A-F]{4}){3}-[0-9A-F]{12})/) {
				$uuid = $1;
				last;
			}
		}
	}

	return $uuid;
}
