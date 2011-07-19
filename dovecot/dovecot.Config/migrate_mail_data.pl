#!/usr/bin/perl

## Migration Script for Mail Services

##################   Input Parameters  #######################
# --moveMail <0 | 1>	A value of "0" will copy the mail messagees into the target root (default: 0)
#						  To preserve the sorce mail data store, use the default value of "0".
#						  A value of "1" will move the messages into the target root thus
#						  deleting the original.
# --cyrusBin <path>		Path to directory containing original cvt_cyrusdb binary
#						  The cvt_cyrusdb binary from the previous system are necessary to convert
#						  the database containing message seen flag settings.
# --database <path>		Path to original cyrus database (default: /var/imap)
#						  This is the path to the Cyrus database from the original system.
# --sourceSpool <path>	Path to original cyrus data store (default: /var/spool/imap)
#						  This is the path to the mail data being migrated.
# --targetSpool <path>	Path to target dovecot mail data store (default: /Library/Server/Mail/Data/mail)
#						  
# Example: migrate_mail_data.pl --cyrusBin "/Volumes/Tiger/usr/bin/cyrus/bin" --database "/Volumes/Tiger/var/imap"
#								--sourceSpool "/Volumes/Tiger/var/spool/imap" --targetSpool "/Library/Server/Mail/Data/mail "

############################# System  Constants #############################
$CAT = "/bin/cat";
$CP = "/bin/cp";
$MV = "/bin/mv";
$RM = "/bin/rm";
$DSCL = "/usr/bin/dscl";
$DU = "/usr/bin/du";
$ECHO = "/bin/echo";
$GREP = "/usr/bin/grep";
$CHOWN = "/usr/sbin/chown";
$LAUNCHCTL = "/bin/launchctl";
$POSTCONF = "/usr/sbin/postconf";
$MKDIR = "/bin/mkdir";
$PLISTBUDDY = "/usr/libexec/PlistBuddy";
$TAR = "/usr/bin/tar";

################################## Consts ###################################
$DOVECOT_CONF="/etc/dovecot/dovecot.conf";
$MIGRATION_LOG= "/Library/Logs/MailDataMigration.log";

################################## Globals ##################################
$g_move_mail		= 0;
$g_cyrus_bin		= "";
$g_database_root	= "/var/imap";
$g_source_root		= "/var/spool/imap";
$g_target_root		= "/Library/Server/Mail/Data/mail";
$g_single_user		= "";
$g_imapd_conf		="";

################################### Flags ###################################
$DEBUG		= 0;
$FUNC_LOG	= 0;

#############################################################################
# and so it goes
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

open ( LOG_FILE, ">> ${MIGRATION_LOG}" ) or die("$MIGRATION_LOG: $!\n");

$g_imapd_conf="/tmp/" . make_random_string() . "-imapd.conf";

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


################################################################################
# get old server version parts

sub get_server_version ($)
{
	my ($VERS) = @_;
	if ( ${FUNC_LOG} ) {
		print( "::get_server_version : S\n"); }

	if ( ${DEBUG} ) {
		printf( "- sourceVersion: %s\n", "${VERS}"); }

	@SRV_VER_PARTS = split(/\./, $VERS); 
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
# do_cyrus_dovecot_migration ()

sub do_cyrus_dovecot_migration ()
{
	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_dovecot_migration : S\n" ); }

	# are we moving or copying messages
	#	default is to move unless purge is set to 0
	$cvt_flag = "-c";
	if ( ${g_move_mail} == 1 )
	{
		$cvt_flag = "-m";
	}

	# migrate default partition
	chdir( "${g_source_root}/user" ) or die "can't chdir ${g_source_root}/user: $!";

	# migrating all or single user
	if ( $g_single_user eq "" )
	{
		@mail_accts = <*>;
	}
	else
	{
		@mail_accts = ( $g_single_user );
	}

	# do the migration
	foreach $user_id (@mail_accts)
	{
		print LOG_FILE "--------------------\n";
		print LOG_FILE "Migrating account: ${user_id} to: ${g_target_root}\n";

		# get first initial from user account name
		#	used for mapping into cyrus database directory layout
		$user_tag = substr( $user_id, 0, 1 );

		if ( ${DEBUG} ) {
			printf( "- verifying user seen db: \"${g_database_root}/user/${user_tag}/${user_id}.seen\"\n" ); }

		if ( path_exists( "${g_database_root}/user/${user_tag}/${user_id}.seen" ) )
		{
			# Convert from skiplist to flat
			print LOG_FILE "-- Seen file conversion: ";
			qx( "${g_cyrus_bin}/cvt_cyrusdb" -C ${g_imapd_conf} "${g_database_root}/user/${user_tag}/${user_id}.seen" skiplist "${g_database_root}/user/${user_tag}/${user_id}.seen.flat" flat >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );

			# Migrate mail data
			if ( ${DEBUG} )
			{
				printf( "-  /usr/bin/cvt_mail_data -g ${cvt_flag} -d ${g_database_root} -s ${g_source_root} -t ${g_target_root} -a ${user_id}\n" );
				qx( /usr/bin/cvt_mail_data -g ${cvt_flag} -d "${g_database_root}" -s "${g_source_root}" -t "${g_target_root}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			} else {
				qx( /usr/bin/cvt_mail_data ${cvt_flag} -d "${g_database_root}" -s "${g_source_root}" -t "${g_target_root}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			}
			
			# clean up
			qx( ${RM} "${g_database_root}/user/${user_tag}/${user_id}.seen.flat" >> "${MIGRATION_LOG}" );
		}
		else
		{
			# Do mail migration without setting any seen flags
			if ( ${DEBUG} )
			{
				printf( "-  /usr/bin/cvt_mail_data -g ${cvt_flag} -d ${g_database_root} -s ${g_source_root} -t ${g_target_root} -a ${user_id}\n" );
				qx( /usr/bin/cvt_mail_data -g ${cvt_flag} -d "${g_database_root}" -s "${g_source_root}" -t "${g_target_root}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			} else {
				qx( /usr/bin/cvt_mail_data ${cvt_flag} -d "${g_database_root}" -s "${g_source_root}" -t "${g_target_root}" -a ${user_id} >> "${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" );
			}
		}

		# do user id to guid mapping
		$user_guid = qx( /usr/bin/cvt_mail_data -i ${user_id} );
		chomp( ${user_guid} );
		if ( substr(${user_guid}, 0, 13) eq "No GUID found" ) {
			qx( chown -R _dovecot:mail "${g_target_root}/${user_id}" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" ); }
		else {
			qx( chown -R _dovecot:mail "${g_target_root}/${user_guid}" >>"${MIGRATION_LOG}" 2>>"${MIGRATION_LOG}" ); }
	}

	if ( ${FUNC_LOG} ) {
		printf( "::do_cyrus_dovecot_migration : S\n" ); }

} # do_cyrus_dovecot_migration


################################################################################
# parse_options()
#	Takes a list of possible options and a boolean indicating
#	whether the option has a value following, and sets up an associative array
#	%opt of the values of the options given on the command line. It removes all
#	the arguments it uses from @ARGV and returns them in @optArgs.

sub parse_options
{
    local (@optval) = @_;
    local ($opt, @opts, %valFollows, @newargs);

    while (@optval)
	{
		$opt = shift(@optval);
		push(@opts,$opt);
		$valFollows{$opt} = shift(@optval);
    }

    @optArgs = ();
    %opt = ();

    arg: while (defined($arg = shift(@ARGV)))
	{
		foreach $opt (@opts)
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
	%script_args = @_;
	while(($theKey, $theVal) = each (%script_args))
	{
		print "key/value: $theKey: $theVal\n";
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
	}
	else
	{
    	if (${DEBUG}) {
			printf( "-- Does not exist\n"); }
	}

    if (${FUNC_LOG}) {
		print( "::path_exists : E\n"); }

	return( $exists );
} # path_exists

################################################################################
# do db clean up

sub do_cleanup ($)
{
    if (${FUNC_LOG}) {
		print( "::do_cleanup : S\n"); }

	my ($in_path) = @_;

	if ( path_exists("${in_path}") )
	{
		print LOG_FILE "Removing source: ${in_path}\n";
		qx( ${RM} -rf "${in_path}" >> ${MIGRATION_LOG} );
	}

	if ( ${FUNC_LOG} ) {
		printf( "::do_cleanup : S\n" ); }
} # do_cleanup


################################################################################
# generate a random 10 char string

sub make_random_string
{
	my $out_string;
	my $my_rand;
	my $my_length = 10;
	my @my_chars = split(" ",
		"a b c d e f g h i j k l m n o
		 p q r s t u v w x y z 0 1 2 3
		 4 5 6 7 8 9");

	srand;

	for ( my $i = 0; $i <= $my_length ; $i++)
	{
		$my_rand = int( rand(31) );
		$out_string .= $my_chars[$my_rand];
	}
	return( $out_string );
} # make_random_string

################################################################################
# Show usage

sub Usage()
{
	print("\n");
	print("--moveMail <0 | 1>    A value of \"1\" will cause migrated mail to be \"moved\" and not copied where the original will be destroyed (default: 0)" . "\n");
	print("--cyrusBin <path>     Path to directory containing original cvt_cyrusdb binary" . "\n");
	print("--database <path>     Path to original cyrus database (default: /var/imap)" . "\n");
	print("--sourceSpool <path>  Path to original cyrus data store (default: /var/spool/imap)" . "\n");
	print("--targetSpool <path>  Path to target dovecot mail data store (default: /Library/Server/Mail/Data/mail)" . "\n");
	print("--accountID <account> Optional account ID if migrating a single user account (default is all accounts)" . "\n");
	print("\n");
	print("Example: migrate_mail_data.pl --cyrusBin \"/Volumes/Tiger/usr/bin/cyrus/bin\" --database \"/Volumes/Tiger/var/imap\" --sourceSpool \"/Volumes/Tiger/var/spool/imap\" --targetSpool \"/Library/Server/Mail/Data/mail\"" . "\n");
	print(" " . "\n");

	if ( LOG_FILE )
	{
		close( LOG_FILE );
	}
	exit( 1 );
} # Usage

################################################################################
# do_migration()

sub do_migration()
{
	%script_args = @_;

	##  Set the globals with the user options
	$g_move_mail	 = $script_args{"--moveMail"};
	$g_cyrus_bin	 = $script_args{"--cyrusBin"};
	$g_database_root = $script_args{"--database"};
	$g_source_root = $script_args{"--sourceSpool"};
	$g_target_root = $script_args{"--targetSpool"};
	$g_single_user = $script_args{"--accountID"};

	## log migration start time
	my $my_time = localtime();
	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Begin Mail Migration: $my_time" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";

	##  log settings
	if ( !($g_move_mail eq "0") ) {
		print LOG_FILE "Move Mail           : No\n"; }
	else {
		print LOG_FILE "Move Mail           : Yes\n"; }
	print LOG_FILE "Database Path       : ${g_database_root}\n";
	print LOG_FILE "Source Path         : ${g_source_root}\n";
	print LOG_FILE "Destination Path    : ${g_target_root}\n";
	print LOG_FILE "Cyrus Binaries Path : ${g_cyrus_bin}\n";
	if ( !($g_single_user eq "") ) {
		print LOG_FILE "Account ID          : ${g_single_user}\n"; }
	else {
		print LOG_FILE "Account ID          : All User Accounts\n"; }
	
	##  verify cyrus binary path
	if ( ! path_exists( "${g_cyrus_bin}/cvt_cyrusdb" ) )
	{
		print LOG_FILE "Error: cvt_cyrusdb does not exist in: ${g_cyrus_bin}.\n";
		print LOG_FILE "Exiting.\n";
		print( "\nError: cvt_cyrusdb does not exist in: ${g_cyrus_bin}.\n" );
		Usage();
	}

	##  verify database path
	if ( ! path_exists( "${g_database_root}" ) )
	{
		print LOG_FILE "Error: Cyrus database path: ${g_database_root} does not exist.\n";
		print LOG_FILE "Exiting.\n";
		print( "\nError: Cyrus database path: ${g_database_root} does not exist.\n" );
		Usage();
	}

	##  verify source path
	if ( ! path_exists( "${g_source_root}/user" ) )
	{
		print LOG_FILE "Error: Cyrus data store path: ${g_source_root}/user does not exist.\n";
		print LOG_FILE "Exiting.\n";
		print( "\nError: Cyrus data store path: ${g_source_root}/user does not exist.\n" );
		Usage();
	}

	##  verify destination volume
	if ( ! path_exists("${g_target_root}") )
	{
		print LOG_FILE "Error: Dovecot destination path: ${g_target_root} does not exist.\n";
		print LOG_FILE "Exiting.\n";
		print( "\nError: Dovecot destination path: ${g_target_root} does not exist.\n" );
		Usage();
	}

	######################
	# cyrus to dovecot
	######################

	# create a temporary imapd.conf
	if ( path_exists( "${g_imapd_conf}" ) )
	{
		qx( ${RM} -rf "${g_imapd_conf}" >> ${MIGRATION_LOG} );
	}

	open ( IMAPD_CONF, ">> ${g_imapd_conf}" ) or die("g_imapd_conf: $!\n");
	print IMAPD_CONF "admins: _cyrus\n";
	print IMAPD_CONF "postmaster: postmaster\n";
	print IMAPD_CONF "configdirectory: ${g_database_root}\n";
	print IMAPD_CONF "defaultpartition: default\n";
	print IMAPD_CONF "partition-default: ${g_source_root}\n";
	close( IMAPD_CONF );

	# migrate cyrus mail data
	do_cyrus_dovecot_migration();

	if ( path_exists( "${g_imapd_conf}" ) )
	{
		qx( ${RM} -rf "${g_imapd_conf}" >> ${MIGRATION_LOG} );
	}

	my $my_time = localtime();
	print LOG_FILE "-------------------------------------------------------------\n";
	print LOG_FILE "Mail Migration Complete: $my_time" . "\n";
	print LOG_FILE "-------------------------------------------------------------\n";
} # do_migration
