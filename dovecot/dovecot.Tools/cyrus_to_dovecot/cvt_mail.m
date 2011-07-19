/*
 *  Contains: Cyrus to Dovecot maildir mail migration
 *	Written by: Michale Dasenbrock
 *  Copyright:  © 2008-2011 Apple Inc., All rights reserved.
 *
 * To Do:
 *	- Gather stats for messages migrated
 *  - Use syslog to log to dovecot info & error files
 *  - May want to do auto-migration of messages found in mailboxes without cyrus index
 *
 * Not For Open Source
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/syslog.h>
#include <getopt.h>

#import <Foundation/NSObject.h>
#include <Foundation/Foundation.h>
#include <Foundation/NSString.h>
#include <OpenDirectory/NSOpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryService/DirServicesConst.h>

#include "cvt_mail.h"

#define	VERSION	"OS X 2.0"

// -- Function Prototypes -------------------------------------------

int cvt_mail_data ( int argc, char **argv );
int set_user_mail_opts ( int argc, char **argv );
int set_dovecot_keywords ( const char *in_cy_header_path, char *in_dest_path );
const char *map_guid( const char *in_user );
const char *map_userid( const char *in_guid );
NSDictionary *get_mail_attribute ( const char *in_guid );
void rename_mailboxes( const char *g_dest_dir );
void fix_mailboxes		( const char *g_dest_dir );
void fix_message_dates	( NSString *in_dir );
void map_mailbox_to_guid ( const char *in_path, const char *in_user );
void set_migration_flag ( const char *in_guid );
void set_migration_flags ( BOOL in_set );
void log_message ( int in_log_lvl, NSString *in_msg, NSError *in_ns_err );
NSDate *get_message_date( NSString *nsStr_in_path );
NSDictionary *get_alt_data_stores ( int in_print );

void list_auto_forwards( const char *in_guid );
void set_auto_forward( const char *guid, const char *fwd_addr );
void reset_auto_forward( const char *in_guid );

void list_alt_data_stores( const char *in_guid );
void set_alt_data_store( const char *guid, const char *fwd_addr );
void reset_alt_data_stores( const char *in_guid );
void set_alt_data_store_tag ( const char *tag, const char *path );
void reset_alt_data_store_tag ( const char *in_tag );

void set_opts_usage ( int in_exit_code );

// ------------------------------------------------------------------

int			g_debug			= 0;
long		g_count			= 0;
long long	g_size			= 0LL;
char		*g_account		= NULL;
char		*g_cyrus_db_dir	= "/var/imap";
char		*g_dest_dir		= "/Library/Server/Mail/Data/mail";
char		*g_user_dir		= "/Library/Server/Mail/Data/users";
FILE		*g_maildirsize	= NULL;

NSAutoreleasePool	*gPool = nil;

// Global cyrus spool path, will append "/user"
char	g_cyrus_spool_dir[ PATH_MAX + 1 ];

// Global flags
int	c_flag	= 0;
int	e_flag	= 0;
int	f_flag	= 0;
int	i_flag	= 0;
int	j_flag	= 0;
int	k_flag	= 0;
int	m_flag	= 0;
int	r_flag	= 0;
int	u_flag	= 0;
int	x_flag	= 0;

// Global seen file data
struct s_seen_data *g_seen_file	= NULL;

// ------------------------------------------------------------------
//

int main ( int argc, char **argv )
{
	gPool = [[NSAutoreleasePool alloc] init];

	if ( geteuid() != 0 ) {
		fprintf( stdout, "%s must be run as root\n", argv[0] );
		exit(0);
	}

	if ( strcasestr(argv[0], "cvt_mail_data" ) ) {
		cvt_mail_data( argc, argv );
		exit(0);
	} else if ( strcasestr(argv[0], "set_user_mail_opts" ) ) {
		set_user_mail_opts( argc, argv );
		exit(0);
	}

	return( 0 );
	[gPool release];
}


int set_user_mail_opts ( int argc, char **argv )
{
	char ch;
	int opt_index = 0;
	int long_val  = 0;

	char *tag		= NULL;
	char *path		= NULL;
	char *user_id	= NULL;
	char *user_guid	= NULL;
	char *fwd_addr	= NULL;
	char *alt_store	= NULL;
	struct option long_options[] = {
		{"alt_store", 1, &long_val, 'a'},
		{"auto_fwd", 1, &long_val, 'f'},
		{"user_guid", 1, &long_val, 'g'},
		{"path", 1, &long_val, 'p'},
		{"tag", 1, &long_val, 't'},
		{"user_id", 1, &long_val, 'u'},
		{0, 0, 0, 0 }
	};

	if ( argc == 1 )
		set_opts_usage(0);

	while ((ch = getopt_long(argc, argv, "ha:f:g:p:t:u:", long_options, &opt_index)) != -1) {
		switch (ch) {
			case 'a':
				alt_store = optarg;
				break;

			case 'f':
				fwd_addr = optarg;
				break;

			case 'g':
				user_guid = optarg;
				break;

			case 'p':
				path = optarg;
				break;

			case 't':
				tag = optarg;
				break;

			case 'u':
				user_id = optarg;
				break;

			case 0:
				switch ( long_val ) {
					case 'a':
						alt_store = optarg;
						break;

					case 'f':
						fwd_addr = optarg;
						break;

					case 'g':
						user_guid = optarg;
						break;

					case 'p':
						path = optarg;
						break;

					case 't':
						tag = optarg;
						break;

					case 'u':
						user_id = optarg;
						break;
				}
				break;
			case 'h':
			default:
				set_opts_usage(0);
		}
	}

	if ( fwd_addr ) {
		const char *guid = NULL;
		if ( !strcasecmp(fwd_addr, "list") ) {
			if ( user_id )
				guid = map_guid( user_id );
			else if ( user_guid )
				guid = user_guid;

			list_auto_forwards( guid );
			return(0);
		} else if ( !strcasecmp(fwd_addr, "reset") ) {
			if ( user_id )
				guid = map_guid( user_id );
			else if ( user_guid )
				guid = user_guid;

			if ( !guid )
				set_opts_usage(1);

			reset_auto_forward( guid );
		} else if (user_id || user_guid) {
			if ( user_id )
				guid = map_guid( user_id );
			else if ( user_guid )
				guid = user_guid;

			if ( !guid )
				set_opts_usage(1);

			set_auto_forward( guid, fwd_addr );
		} else
			set_opts_usage(1);
	} else if ( alt_store ) {
		const char *guid = NULL;
		if ( !strcasecmp(alt_store, "list") ) {
			if ( user_id )
				guid = map_guid( user_id );
			else if ( user_guid )
				guid = user_guid;

			list_alt_data_stores( guid );
			return(0);
		} else if ( !strcasecmp(alt_store, "list-tags") ) {
			get_alt_data_stores(1);
		} else if ( !strcasecmp(alt_store, "set-tag") ) {
			if ( !path || !tag )
				set_opts_usage(1);

			set_alt_data_store_tag( tag, path );
			return(0);
		} else if ( !strcasecmp(alt_store, "reset-tag") ) {
			if ( !tag )
				set_opts_usage(1);

			reset_alt_data_store_tag( tag );
			return(0);
		} else if ( !strcasecmp(alt_store, "reset") ) {
			if ( user_id )
				guid = map_guid( user_id );
			else if ( user_guid )
				guid = user_guid;

			if ( !guid )
				set_opts_usage(1);

			reset_alt_data_stores( guid );
		} else if (user_id || user_guid) {
			if ( user_id )
				guid = map_guid( user_id );
			else if ( user_guid )
				guid = user_guid;

			if ( !guid )
				set_opts_usage(1);

			set_alt_data_store( guid, alt_store );
		} else
			set_opts_usage(1);
	}
	return(0);
}

int cvt_mail_data ( int argc, char **argv )
{
	int	ch;
	char *value = NULL;
	char *tmp_cyrus_spool_dir = "/var/spool/imap";

	gPool = [[NSAutoreleasePool alloc] init];

	if ( geteuid() != 0 ) {
		fprintf( stdout, "%s must be run as root\n", argv[0] );
		exit(0);
	}

	while ( (ch = getopt(argc, argv, "cemgva:d:f:j:k:r:s:i:t:u:x:")) != EOF ) {
		switch( ch ) {
			case 'a':
				// User account ID
				g_account = optarg;
				break;

			case 'd':
				// Cyrus imap database directory
				g_cyrus_db_dir = optarg;
				break;

			case 'i':
				// User account ID
				g_account = optarg;
				i_flag++;
				break;

			case 'r':
				// User account ID
				g_dest_dir = optarg;
				r_flag++;
				break;

			case 's':
				// Cyrus imap data spool directory
				tmp_cyrus_spool_dir = optarg;
				break;

			case 't':
				// Destination maildir data directory
				g_dest_dir = optarg;
				break;

			case 'u':
				// User account ID
				g_account = optarg;
				u_flag++;
				break;

			case 'c':
				// 'C'opy flag
				c_flag = 1;
				break;

			case 'e':
				// D'e'lete flag
				e_flag = 1;
				break;

			case 'm':
				// 'M'ove flag
				m_flag = 1;
				break;

			case 'f':
				// Fix directory UIDs
				g_dest_dir = optarg;
				f_flag = 1;
				break;

			case 'v':
				fprintf( stdout, "%s: Version: %s\n", argv[0], VERSION );
				[gPool release];
				exit( 0 );
				break;

			case 'g':
				g_debug++;
				break;

			case 'j':
				value = optarg;
				j_flag++;
				break;

			case 'k':
				value = optarg;
				k_flag++;
				break;

			case 'x':
				// Fix message received dates
				g_dest_dir = optarg;
				x_flag = 1;
				break;

			case '?':
			case 'h':
			default:
				usage( 0 );	
		}
	}
	argc -= optind;
	argv += optind;

	// ------------------------------------------------------------------
	// Print GUID for user account

	if ( i_flag ) {
		if ( g_account != NULL ) {
			const char *map = map_guid( g_account );
			if ( map != NULL )
				fprintf( stdout, "%s\n", map );
			else
				fprintf( stdout, "No GUID found for: %s\n", g_account );

			[gPool release];
			exit( 0 );
		} else {
			fprintf( stdout, "*** Error: Empty user account ID\n" );
			usage(1);
		}
	}

	// ------------------------------------------------------------------
	// Print GUID for user account

	if ( u_flag ) {
		if ( g_account != NULL ) {
			const char *map = map_userid( g_account );
			if ( map != NULL )
				fprintf( stdout, "%s\n", map );
			else
				fprintf( stdout, "No user id found for: %s\n", g_account );

			[gPool release];
			exit( 0 );
		} else {
			fprintf( stdout, "*** Error: Empty user account ID\n" );
			usage(1);
		}
	}

	// ------------------------------------------------------------------
	// Rename user directories to associated GUIDs

	if ( r_flag ) {
		if ( g_dest_dir != NULL ) {
			rename_mailboxes( g_dest_dir );

			[gPool release];
			exit( 0 );
		} else {
			fprintf( stdout, "*** Error: Empty directory\n" );
			usage(1);
		}
	}

	// ------------------------------------------------------------------
	// Rename user directories to associated GUIDs

	if ( f_flag ) {
		if ( g_dest_dir != NULL ) {
			fix_mailboxes( g_dest_dir );

			[gPool release];
			exit( 0 );
		} else {
			fprintf( stdout, "*** Error: empty directory\n" );
			usage(1);
		}
	}

	// ------------------------------------------------------------------
	// Set user migration flag

	if ( j_flag ) {
		if ( !value ) {
			fprintf( stdout, "*** Error: missing required argument to \'j\' option\n" );
			usage(1);
		}

		set_migration_flag( value );
		[gPool release];
		exit( 0 );
	}

	// ------------------------------------------------------------------
	// Reset user migration flags

	if ( k_flag ) {
		int flag = 0;
		if ( !value ) {
			fprintf( stdout, "*** Error: missing required argument to \'k\' option\n" );
			usage(1);
		}

		if ( !strcmp( value, "set" ) )
			flag = 1;
		else if ( strcmp( value, "reset" ) ) {
			fprintf( stdout, "*** Error: bad argument to \'k\' option\n" );
			usage(1);
		}

		set_migration_flags( flag );
		[gPool release];
		exit( 0 );
	}

	// ------------------------------------------------------------------
	// Rename user directories to associated GUIDs

	if ( x_flag ) {
		if ( g_dest_dir != NULL ) {
			fix_message_dates( [NSString stringWithUTF8String: g_dest_dir] );

			[gPool release];
			exit( 0 );
		} else {
			fprintf( stdout, "*** Error: Empty directory\n" );
			usage(1);
		}
	}

	// ------------------------------------------------------------------
	// Do arg verification

	// Verify cyrus database path
	if ( g_cyrus_db_dir == NULL ) {
		fprintf( stdout, "*** Error: Empty cyrus database path\n" );
		usage(1);
	} else {
		if ( verify_path( g_cyrus_db_dir ) != 0 ) {
			fprintf( stdout, "*** Error: Invalid cyrus database path: %s\n", g_cyrus_db_dir );
			usage(1);
		}
	}

	// Verify cyrus spool path
	if ( tmp_cyrus_spool_dir == NULL ) {
		fprintf( stdout, "*** Error: Empty cyrus data spool path\n" );
		usage(1);
	} else {
		if ( verify_path( tmp_cyrus_spool_dir ) != 0 ) {
			fprintf( stdout, "*** Error: Invalid cyrus database path: %s\n", tmp_cyrus_spool_dir );
			usage(1);
		}
	}

	// Verify maildir destination spool path
	if ( g_dest_dir == NULL ) {
		fprintf( stdout, "*** Error: Empty destination directory\n" );
		usage(1);
	} else {
		if ( verify_path( g_dest_dir ) != 0 ) {
			fprintf( stdout, "*** Error: Invalid destination directory: %s\n", g_dest_dir );
			usage(1);
		}
	}

	// Verify user account ID
	if ( g_account == NULL )
	{
		fprintf( stdout, "*** Error: Empty user account ID\n" );
		usage(1);
	} else {
		// Verify user path by appending /user/account to spool path
		snprintf( g_cyrus_spool_dir, PATH_MAX, "%s/user/%s", tmp_cyrus_spool_dir, g_account );

		if ( verify_path( g_cyrus_spool_dir ) != 0 ) {
			fprintf( stdout, "*** Error: Could not verify user account path: %s\n", g_cyrus_spool_dir );
			usage(1);
		}

		// Set global spool path to <spool path>/user
		snprintf( g_cyrus_spool_dir, PATH_MAX, "%s/user", tmp_cyrus_spool_dir );
	}

	if ( (c_flag +  e_flag + m_flag) == 0 )
		c_flag = 1;
	else if ( (c_flag +  e_flag + m_flag) != 1 ) {
		fprintf( stdout, "*** Error: You can only choose of these options [-c, -e or -m]\n" );
		usage(1);
	}

	chdir( g_cyrus_spool_dir );
	scan_account( g_cyrus_spool_dir, g_dest_dir, g_account );
	free_seen_file();

	fprintf( stdout, "-------------\n" );
	fprintf( stdout, "- totals for: %s\n",  g_account );
	fprintf( stdout, "  total: messages: %lu size: %llu bytes\n",  g_count, g_size );

	if ( g_maildirsize != NULL ) {
		fclose( g_maildirsize );
		g_maildirsize = NULL;
	}

	if ( g_debug ) {
		fprintf( stdout, "Finished migrating user account\n" );
	}

	return( 0 );
} // main


// ------------------------------------------------------------------
//

void set_opts_usage ( int in_exit_code )
{
	if ( in_exit_code )
		fprintf( stdout, "Error: Missing required argument\n");
	
	fprintf( stdout, "\nUsage:\n");
	fprintf( stdout, "  set_user_mail_opts [options]\n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store list\n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store list-tags\n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store set-tag -t, --tag <tag> -p, --path <path>\n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store reset-tag -t, --tag <tag> \n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store <store tag> -u, --user_id <user id>\n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store <store tag> -g, --user_guid <user guid>\n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store reset -u, --user_id <user id>\n");
	fprintf( stdout, "  set_user_mail_opts -a, --alt_store reset -g, --user_guid <user guid>\n");

	fprintf( stdout, "  set_user_mail_opts -f, --auto_fwd list\n");
	fprintf( stdout, "  set_user_mail_opts -f, --auto_fwd <email addr> -u, --user_id <user id>\n");
	fprintf( stdout, "  set_user_mail_opts -f, --auto_fwd <email addr> -g, --user_guid <user guid>\n");
	fprintf( stdout, "  set_user_mail_opts -f, --auto_fwd reset -u, --user_id <user id>\n");
	fprintf( stdout, "  set_user_mail_opts -f, --auto_fwd reset -g, --user_guid <user guid>\n");

	fprintf( stdout, "\nOptions:\n");
	fprintf( stdout, "  -a, --alt_store list        list all account with alternate mail store locations set\n");
	fprintf( stdout, "  -a, --alt_store list-tags   list all alternate mail store locations with tags\n");
	fprintf( stdout, "  -a, --alt_store <store tag> -u, --user_id <user id>\n");
	fprintf( stdout, "                              set <user id> to alternate mail store <store tag>\n");
	fprintf( stdout, "  -a, --alt_store <store tag> -g, --user_guid <user guid>\n");
	fprintf( stdout, "                              set <user guid> to alternate mail store <store tag>\n");
	fprintf( stdout, "  -a, --alt_store reset       -u, --user_id <user id>\n");
	fprintf( stdout, "                              reset <user id> to default mail store <store tag>\n");
	fprintf( stdout, "  -a, --alt_store reset       -g, --user_guid <user guid>\n");
	fprintf( stdout, "                              reset <user guid> to default mail store <store tag>\n");

	fprintf( stdout, "  -f, --auto_fwd list         list all account with email autoforwarding enabled\n");
	fprintf( stdout, "  -f, --auto_fwd <email addr> -u, --user_id <user id>\n");
	fprintf( stdout, "                              set <user id> to autoforward to <email addr>\n");
	fprintf( stdout, "  -f, --auto_fwd <email addr> -g, --user_guid <user guid>\n");
	fprintf( stdout, "                              set <user guid> to autoforward to <email addr>\n");
	fprintf( stdout, "  -f, --auto_fwd reset        -u, --user_id <user id>\n");
	fprintf( stdout, "                              reset <user id> to no email autoforwarding\n");
	fprintf( stdout, "  -f, --auto_fwd reset        -g, --user_guid <user guid>\n");
	fprintf( stdout, "                              reset <user guid> to no email autoforwarding\n");

	[gPool release];
	exit( in_exit_code );
} // set_opts_usage

// ------------------------------------------------------------------
//

void usage ( int in_exit_code )
{
	fprintf( stdout, "\nUsage: cvt_mail_data -a <acct_id> [options]\n");

	fprintf( stdout, "    Required\n" );
	fprintf( stdout, "      -a <user account>     user ID of account to be migrated\n" );
	fprintf( stdout, "\n" );
	fprintf( stdout, "    Default: (override if necessary)\n" );
	fprintf( stdout, "      -d <db dir>           path to cyrus imap database (default: %s)\n", g_cyrus_db_dir );
	fprintf( stdout, "      -s <spool dir>        path to cyrus imap data spool (default: /var/spool/imap)\n" );
	fprintf( stdout, "      -t <destination dir>  path to dovecot maildir directory (default: %s)\n", g_dest_dir );
	fprintf( stdout, "\n" );
	fprintf( stdout, "    Choose one:\n" );
	fprintf( stdout, "      -c                    copy mail messages to new destination leaving original (default)\n" );
	fprintf( stdout, "      -e                    copy mail messages to new destination deleting original\n" );
	fprintf( stdout, "      -m                    move mail messages to new destination\n" );
	fprintf( stdout, "\n" );
	fprintf( stdout, "    Optional:\n" );
	fprintf( stdout, "      -g                    debug mode\n\n" );
	fprintf( stdout, "    Single Function:\n" );
	fprintf( stdout, "      -i <user account>     print GUID for user account\n" );
	fprintf( stdout, "      -u <GUID>             print user account for GUID\n" );
	fprintf( stdout, "      -r <dir>              rename mailboxes to GUID in directory <dir>\n" );
	fprintf( stdout, "      -V                    print version\n" );
	fprintf( stdout, "      -h                    print this message\n\n" );

	[gPool release];
	exit( in_exit_code );
}

// ------------------------------------------------------------------
//
// verify_path ()

int verify_path ( const char *in_path )
{
	DIR	*dir	= NULL;
	
	if ( dir = opendir( in_path ) ) {
		closedir( dir );
		return( 0 );
	}

	return( 1 );

} // verify_path

// ------------------------------------------------------------------
// fts_escape ()

static void fts_escape ( char *out_str, const char *in_orig )
{
	char *p = out_str;
	static const char *hexchars = "0123456789abcdef";

	while ( *in_orig != '\0' ) {
		unsigned char c = *in_orig;
		if ((c >= 'A' && c <= 'Z') ||
		    (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9'))
			*p++ = c;
		else {
			*p++ = '%';
			*p++ = hexchars[(c >> 4) & 0xf];
			*p++ = hexchars[c & 0xf];
		}
		++in_orig;
	}
} // fts_escape

// ------------------------------------------------------------------
// set_fts_update_index_file ()

static void set_fts_update_index_file ( const char * in_src_path, const char *in_dst_path )
{
	static char path[PATH_MAX];
	static char account[PATH_MAX];
	static char mailbox[PATH_MAX];

	// get the user ID from the source path
	NSString *ns_str = [NSString stringWithCString: in_src_path encoding: NSUTF8StringEncoding];
	NSArray *ns_arry = [ns_str componentsSeparatedByCharactersInSet: [NSCharacterSet characterSetWithCharactersInString: @"/"]];

	memset( account, 0, PATH_MAX );
	fts_escape( account, [[ns_arry objectAtIndex: 0]UTF8String] );

	// get the '.' delimited mailbox name
	ns_str = [NSString stringWithCString: in_dst_path encoding: NSUTF8StringEncoding];
	ns_str = [ns_str stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString: @"/"]];
	ns_arry = [ns_str componentsSeparatedByCharactersInSet: [NSCharacterSet characterSetWithCharactersInString: @"/"]];

	memset( mailbox, 0, PATH_MAX );
	if ( [ns_arry count] <= 1 )
		strlcpy( mailbox, "INBOX", PATH_MAX );
	else
		fts_escape( mailbox, [[[ns_arry objectAtIndex: 1] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString: @"."]] UTF8String] );

	snprintf( path, PATH_MAX, "/var/db/dovecot.fts.update/%s.%s", account, mailbox );

	if ( g_debug )
		fprintf( stdout, "creating fts update index file: %s\n", path );

	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if ( (fd < 0) && (errno != EEXIST) )
		fprintf( stdout, "fts: open(%s, O_CREAT) failed: %m", path );
} // set_fts_update_index_file

// ------------------------------------------------------------------
// scan_account ()

int scan_account ( char *in_cy_spool_dir, char *in_dst_root, char *in_acct_dir )
{
	DIR			   *p_dir		= NULL;
	struct dirent  *dir_entry;
	struct stat		stat_buf;

	static int is_root_mailbox				= 1;
	static char src_path	[ PATH_MAX + 1 ]	= "";
	static char dst_path	[ PATH_MAX + 1 ]	= "";
	static char full_path	[ PATH_MAX + 1 ]	= "";
	static char src_mb_path	[ PATH_MAX + 1 ]	= "";

	char *cc = NULL;
	int rc = 0;
	const char *guid_map = NULL;
	const char *dst_acct = NULL;

	if ( is_root_mailbox ) {
		if ( verify_path( in_acct_dir ) != 0 ) {
			fprintf( stdout, "*** Warning: unable to verify mailbox path: %s\n", in_acct_dir );
			[gPool release];
			exit(1);
		}

		char	seen_path[ PATH_MAX + 1 ];
		snprintf( seen_path, PATH_MAX, "%s/user/%c/%s.seen.flat", g_cyrus_db_dir, g_account[0], g_account );

		read_seen_file( seen_path );
		if ( !g_seen_file ) {
			if ( g_debug ) {
				fprintf( stdout, "*** Warning: No seen file found for for: %s\n", g_account ); }
		}
	}

	// Drop in if it's a directory
	if ( (stat( in_acct_dir, &stat_buf ) == 0) &&
		 ((stat_buf.st_mode & S_IFMT) == S_IFDIR) )
	{
		// Add '/' if we now have a path name
		if ( strlen( src_path ) != 0 )
			strcat( src_path, "/" );

		// Append source mailbox to path
		strcat( src_path, in_acct_dir );

		// Mailboxes are separate with a '.'
		if ( strlen( dst_path ) != 0 )
			strcat( dst_path, "." );

		// Append destination mailbox name, either account ID or GUID if it exists
		if ( is_root_mailbox && (guid_map = map_guid( in_acct_dir )) != NULL )
			dst_acct = guid_map;
		else
			dst_acct = in_acct_dir;

		strcat( dst_path, dst_acct );

		// Append a '/' to the base mailbox name
		//	We should only hit this once per account
		if ( !strcmp( dst_path, dst_acct) )
			strcat( dst_path, "/" );

		src_mb_path[0]='\0';
		strcat( src_mb_path, in_cy_spool_dir );
		strcat( src_mb_path, "/" );
		strcat( src_mb_path, src_path );

		// Make the full path
		snprintf( full_path, PATH_MAX, "%s/%s", in_dst_root, dst_path );

		// create maildir directories
		create_maildir_dirs( in_dst_root, dst_path, is_root_mailbox );

		// Migrate cyrus quota info
		if ( is_root_mailbox )
			set_quota( in_dst_root, dst_path );

		// Set subscribed mailboxes
		if ( is_root_mailbox )
			set_subscribe( in_dst_root, dst_path );

		// copy and/or migrate the mailfiles
		migrate_mail( src_mb_path, full_path, is_root_mailbox );

		// create update index file for background processing
		set_fts_update_index_file( src_path, dst_path );

		if ( is_root_mailbox )
			is_root_mailbox = 0;

		// Scan mailboxes
		if ( chdir( in_acct_dir ) == 0 ) {
			if ( p_dir = opendir( "." ) ) {
				// Recursively scan across all mailboxes 
				while( (dir_entry = readdir(p_dir)) ) {
					// Skip the '.' & '..' directories
					if ( strcmp( dir_entry->d_name, "." ) &&  strcmp( dir_entry->d_name, "..") )
						scan_account( in_cy_spool_dir, in_dst_root, dir_entry->d_name );
				}
				closedir( p_dir );
			}

			// Backing out, terminate source mailbox path
			if ( cc = strrchr( src_path, '/' ) )
				*cc = '\0';

			// Backing out, terminate destination mailbox path
			if ( cc = strrchr( dst_path, '.') )
				*cc='\0';

			chdir( ".." );
		}
	}

	return( rc );
} // scan_account


// ------------------------------------------------------------------
//
// set_quota ()

int set_quota ( char *in_dst_root, char *in_dir )
{
	FILE		*src_file	= NULL;
	long long	max_quota	= 0;
	char		*p_line		= NULL;
	char		src_path[ PATH_MAX + 1 ];
	char		dst_path[ PATH_MAX + 1 ];

	// Get source path from base path + first letter of account name + account name
	//	ie. /var/imap/quota/j/user.joe
	snprintf( src_path, PATH_MAX, "%s/quota/%c/user.%s", g_cyrus_db_dir, g_account[0], g_account );

	// Create destination path from target directory + directory name
	snprintf( dst_path, PATH_MAX, "%s/%s/maildirsize", in_dst_root, in_dir );

	// Open soruce file
	src_file = fopen( src_path, "r" );
	if ( src_file == NULL ) {
		fprintf( stdout, "*** Warning: Could not open quota file: %s\n", src_path );
		return( -1 );
	}

	g_maildirsize = fopen( dst_path, "w" );
	if ( g_maildirsize == NULL ) {
		fprintf( stdout, "*** Warning: Could not open quota file: %s\n", dst_path );
		fclose( src_file );
		return( -1 );
	}

	// read "quota used"
	//	- this value is not used, will calculate from actual messages migrated
	p_line = read_line( src_file );

	// Read "max quota"
	p_line = read_line( src_file );
	if ( p_line != NULL ) {
		// convert K to bytes
		if ( atol( p_line ) != 2147483647 )
			max_quota = atoll( p_line ) * 1024LL;
		free( p_line );
		p_line = NULL;
	}

	// set max quota in .../maildirsize file
	fprintf( stdout, "Setting mail quota to: %lld (bytes)\n", max_quota );
	fprintf( g_maildirsize, "%lldS\n", max_quota );

	fclose( src_file );

	return( 0 );

} // set_quota


// ------------------------------------------------------------------
//
// set_subscribe ()

int set_subscribe ( char *in_dst_root, char *in_dir )
{
	int		len			= 0;
	char   *tab_char	= NULL;
	FILE	*src_file;
	FILE	*dst_file;
	char   *line_buf	= NULL;
	char	src_path[ PATH_MAX + 1 ];
	char	dst_path[ PATH_MAX + 1 ];

	// Get source path from base path + first letter of account name + account name
	//	ie. /var/imap/user/j/joe.sub
	snprintf( src_path, PATH_MAX, "%s/user/%c/%s.sub", g_cyrus_db_dir, g_account[0], g_account );

	// Create destination path from target directory + directory name
	snprintf( dst_path, PATH_MAX, "%s/%s/subscriptions", in_dst_root, in_dir );

	// Get length of: "user." + acct name + '.'
	len = strlen( g_account ) + 6;

	// Open soruce file
	src_file = fopen( src_path, "r" );
	if ( src_file == NULL ) {
		fprintf( stdout, "*** Warning: Could not open subscribe file: %s\n", src_path );
		return( -1 );
	}

	dst_file = fopen( dst_path, "w" );
	if ( dst_file == NULL ) {
		fprintf( stdout, "*** Warning: Could not open subscribe file: %s\n", dst_path );
		fclose( src_file );
		return( -1 );
	}

	while ( (line_buf = read_line( src_file )) != NULL ) {
		if ( strlen( line_buf ) > 0 ) {
			tab_char = strrchr( &line_buf[len], '\t' );
			if ( tab_char )
				*tab_char = '\0';
		}
		// Write just the mailbox name
		fprintf( dst_file, "%s\n", &line_buf[len] );

		free( line_buf );
		line_buf = NULL;
	}

	fclose( dst_file );
	fclose( src_file );

	return( 0 );

} // set_subscribe


// ------------------------------------------------------------------
//
// create_maildir_dirs ()

int create_maildir_dirs ( char *root, char *dir, int is_root )
{
	char	mb_root[ PATH_MAX + 1 ];
	char	mb_cur[ PATH_MAX + 1 ];
	char	mb_new[ PATH_MAX + 1 ];
	char	mb_tmp[ PATH_MAX + 1 ];
	char	mb_mdir[ PATH_MAX + 1 ];

	snprintf( mb_root, PATH_MAX, "%s/%s", root, dir );
	snprintf( mb_cur, PATH_MAX, "%s/cur", mb_root );
	snprintf( mb_new, PATH_MAX, "%s/new", mb_root );
	snprintf( mb_tmp, PATH_MAX, "%s/tmp", mb_root );
	snprintf( mb_mdir, PATH_MAX, "%s/maildirfolder", mb_root );

	mkdir( mb_root, S_IRWXU );
	mkdir( mb_cur, S_IRWXU );
	mkdir( mb_new, S_IRWXU );
	mkdir( mb_tmp, S_IRWXU );

	if ( !is_root ) {
		FILE	*a_file = NULL;

		a_file = fopen( mb_mdir, "w");
		if ( a_file != NULL )
			fclose( a_file );
	}
		
	return( 0 );

} // create_maildir_dirs


// ------------------------------------------------------------------
//
// migrate_message ()

int migrate_message ( char *in_src, unsigned long in_date, char *in_dst, char *in_flags, unsigned long *out_size )
{
	NSError			*nsError	= nil;
	NSString		*nsStr_dst	= nil;
	struct stat		file_stat;
	struct timeval	time_val[ 2 ];
	NSFileManager	*nsFileMgr	= [NSFileManager defaultManager];

	stat( in_src, &file_stat );
	*out_size = file_stat.st_size;

	// Destination path name
	nsStr_dst = [NSString stringWithFormat: @"%s%llu%s", in_dst, file_stat.st_size, in_flags ];
	if ( c_flag == 1 || e_flag == 1 ) {
		if ( g_debug )
			fprintf( stdout, "copying: %s to: %s\n", in_src, [nsStr_dst UTF8String] );

		if ( [nsFileMgr copyItemAtPath: [NSString stringWithUTF8String: in_src] toPath: nsStr_dst error: &nsError ] == NO )
			fprintf( stdout, "Error: Failed to move: %s (%s)\n", in_src, [[nsError localizedFailureReason] UTF8String] );

		if ( e_flag == 1 ) {
			if ( [nsFileMgr removeItemAtPath: [NSString stringWithUTF8String: in_src] error: &nsError ] == NO )
				fprintf( stdout, "Error: Failed to delete: %s (%s)\n", in_src, [[nsError localizedFailureReason] UTF8String] );
		}
	} else {
		if ( g_debug )
			fprintf( stdout, "moving: %s  to: %s\n", in_src, [nsStr_dst UTF8String] );

		if ( [nsFileMgr moveItemAtPath: [NSString stringWithUTF8String: in_src] toPath: nsStr_dst error: &nsError ] == NO )
			fprintf( stdout, "Error: Failed to move: %s (%s)\n", in_src, [[nsError localizedFailureReason] UTF8String] );
	}

	// fix file mod times, this is the imap "received time"
	time_val[ 0 ].tv_sec = file_stat.st_atime;
	time_val[ 1 ].tv_sec = in_date;
	utimes( [nsStr_dst UTF8String], time_val );
	
	return( 0 );

} // migrate_message


// ------------------------------------------------------------------
//
// migrate_mail ()

int migrate_mail ( char *in_src_path, char *in_dest_path, int is_root )
{
	unsigned int	i			= 0;
	unsigned long	ver			= 0;
	unsigned int	len			= 0;
	unsigned long	msg_size	= 0;
	long long		total_bytes	= 0LL;
	long			total_msgs	= 0;
	char		   *mb_path		= NULL;
	FILE		   *hdr_file	= NULL;
	FILE		   *uid_file	= NULL;
	DIR			   *p_dir		= NULL;
	struct dirent  *dir_entry;
	char			keyword		  [ 2 ];
	char			dst_file_path [ PATH_MAX + 1 ];
	char			src_mb_path   [ PATH_MAX + 1 ];
	char			cy_index_path [ PATH_MAX + 1 ];
	char			cy_header_path[ PATH_MAX + 1 ];
	char			src_msg_path  [ PATH_MAX + 1 ];
	char			msg_flags_str [ PATH_MAX + 1 ];

	struct index_header		cy_header;
	struct index_entry		cy_index_entry;

	if ( g_debug ) {
		fprintf( stdout, "Migrating mailbox: %s  to: %s\n",  in_src_path, in_dest_path ); }

	// Get cyrus mailbox index file path
	snprintf( cy_index_path, PATH_MAX, "%s/cyrus.index", in_src_path );

	// Get cyrus mailbox header file path
	snprintf( cy_header_path, PATH_MAX, "%s/cyrus.header", in_src_path );

	// Get the cyrus index version
	if ( hdr_file = fopen( cy_index_path, "r") ) {
		unsigned long	uidvalidity = 0;

		// get the version number for the cyrus minor_version in header
		memset( &cy_header, 0, sizeof( cy_header ) );
		fread( &cy_header, 3*4, 1, hdr_file );

		ver = ntohl( cy_header.minor_version );
		fseek( hdr_file, 0, SEEK_SET );

		if ( g_debug ) {
			fprintf( stdout, "cyrus index version: %lu\n", ver ); }

		// check for the only two we care about
		switch ( ver ) {
			case 2:
				fread( &cy_header, 44, 1, hdr_file );
				uidvalidity = (unsigned long)ntohl(cy_header.pop3_last_login);
				break;
			case 3:
				fread( &cy_header, 56, 1, hdr_file );
				uidvalidity = (unsigned long)ntohl(cy_header.pop3_last_login);
				break;
			case 4:
				fread( &cy_header, 76, 1, hdr_file );
				uidvalidity = (unsigned long)ntohl(cy_header.pop3_last_login);
				break;
			case 6:
				fread( &cy_header, 80, 1, hdr_file );
				uidvalidity = (unsigned long)ntohl(cy_header.uidvalidity);
				break;
			case 9:
				fread( &cy_header, 96, 1, hdr_file );
				uidvalidity = (unsigned long)ntohl(cy_header.uidvalidity);
				break;
			default:
				fprintf( stdout, "Unsupported cyrus mailbox header version: %lu\n",  ver );
				[gPool release];
				exit(1);
		}

		sprintf( dst_file_path, "%s/dovecot-uidlist", in_dest_path );
		uid_file = fopen( dst_file_path, "w" );
		if ( uid_file == NULL ) {
			fprintf( stdout, "*** Error: could not open: %s\n",  dst_file_path );
			[gPool release];
			exit(1);
		}

		// Copy over uid data from cyrus to maildir mailbox
		fprintf( uid_file, "1 %lu %lu\n", uidvalidity, (unsigned long)ntohl(cy_header.last_uid) + 1 );

		// Parse cyrus seen db file
		strlcpy( src_mb_path, in_src_path, PATH_MAX );
		len = strlen( g_cyrus_spool_dir );

		// Just get the relative mailbox path
		mb_path = src_mb_path + len;

		// Convert any /'s to .'s in the mailbox path name
		for ( len = 0; len < strlen( mb_path ); len++ ) {
			if ( mb_path[ len ]=='/' )
				mb_path[ len ]='.';
		}
		mb_path++;

		// Do the seen file parsing
		parse_seen_file( mb_path, uidvalidity );

		memset( &cy_index_entry, 0, sizeof( cy_index_entry ) );
		fseek( hdr_file, htonl(cy_header.start_offset), SEEK_SET );

		// Scan through the cyrus header file and migrate a mail file for each entry
		while( fread( &cy_index_entry, ntohl( cy_header.record_size ), 1, hdr_file) ) {
			// Make target file path
			sprintf( dst_file_path, "%s/cur/%lu.cyrus.%lu,S=",
									 in_dest_path,
									(unsigned long)ntohl( cy_index_entry.internaldate ),
									(unsigned long)ntohl( cy_index_entry.uid ) );

			// Set maildir flags
			strlcpy( msg_flags_str, ":2,", PATH_MAX );

			// Draft flag
			if ( (ntohl(cy_index_entry.system_flags) & FLAG_DRAFT) )
				strcat( msg_flags_str, "D" );

			// Flagged flag
			if ( (ntohl(cy_index_entry.system_flags) & FLAG_FLAGGED) )
				 strcat( msg_flags_str, "F" );

			// Draft flag
			if ( (ntohl(cy_index_entry.system_flags) & FLAG_ANSWERED) )
				 strcat( msg_flags_str, "R" );

			// Seen flag
			if ( is_seen( (unsigned long)(ntohl(cy_index_entry.uid))) )
				strcat( msg_flags_str, "S" );

			// Draft flag
			if ( (ntohl(cy_index_entry.system_flags) & FLAG_DELETED) )
				 strcat( msg_flags_str, "T" );

			// Set dovecot keywords
			keyword[ 1 ] = '\0';
			for ( i = 0; i < MAX_USER_FLAGS && i <= 'z'-'a'; i++) {
				if ( (htonl( cy_index_entry.user_flags[i / 8 ]) & (1 << (i % 8))) != 0 ) {
					keyword[ 0 ] = 'a' + i;
					strcat( msg_flags_str, keyword );
				}
			}

			// Full source path
			sprintf( src_msg_path, "%s/%lu.", in_src_path, (unsigned long)ntohl(cy_index_entry.uid) );

			// Now do the actual copy/move of the cyrus mail file
			migrate_message( src_msg_path, (unsigned long)ntohl(cy_index_entry.internaldate), dst_file_path, msg_flags_str, &msg_size );

			// Set uid in dovecot dovecot-uidlist
			fprintf( uid_file, "%lu %s%lu\n", (unsigned long)ntohl(cy_index_entry.uid), strrchr(dst_file_path, '/') + 1, msg_size );

			total_bytes += msg_size;
			total_msgs++;
		}

		if ( total_bytes != 0 ) {
			if ( g_maildirsize != NULL )
				fprintf( g_maildirsize, "%-13lld %-13ld\n", total_bytes, total_msgs );
		}

		fprintf( stdout, "- mailbox: %s\n",  in_src_path );
		fprintf( stdout, "  messages: %lu size: %llu bytes\n",  total_msgs, total_bytes );

		g_count += total_msgs;
		g_size += total_bytes;

		fclose( hdr_file );
		fclose( uid_file );
	} else {
		// Hmmmm, no cyrus.index file here
		//	Do we even want to mess with this mailbox or just log it and move on....
		//	For now, check for mail and log that there may be un-migrated messages

		// Checking if there may be mail here
		if ( p_dir = opendir(in_src_path) ) {
			while ( (dir_entry = readdir(p_dir)) ) {
				if ( (dir_entry->d_name[strlen( dir_entry->d_name ) - 1] == '.') && (dir_entry->d_name[0] != '.') ) {
					// There may be messages here
					fprintf( stdout, "*** Warning: Missing cyrus index file and mailbox may cntain messages in: %s\n",  in_src_path );
					break;
				}
			}
			closedir( p_dir );
		}
	}

	// Now let's write us some dovecot keywords
	return( set_dovecot_keywords( cy_header_path, in_dest_path ) );

} // migrate_mail


// ------------------------------------------------------------------
//
// set_dovecot_keywords ()

int set_dovecot_keywords ( const char *in_cy_header_path, char *in_dest_path )
{
	unsigned int	i		= 0;
	char		   *keyword			= NULL;
	FILE		   *keyword_file	= NULL;
	FILE		   *hdr_file		= NULL;
	char			dst_file_path [ PATH_MAX + 1 ];
	char			line_buf[ BUF_SIZE ];

	// Now let's write us some dovecot keywords
	hdr_file = fopen( in_cy_header_path, "r" );
	if ( hdr_file != NULL ) {
		sprintf( dst_file_path, "%s/%s", in_dest_path, "dovecot-keywords" );

		// Skip past header tag
		fseek( hdr_file, sizeof( MAILBOX_HEADER_MAGIC ) - 1, SEEK_SET );

		// Get second line past header tag
		if ( (fgets( line_buf, sizeof( line_buf ), hdr_file ) != NULL) &&
			 (fgets( line_buf, sizeof( line_buf ), hdr_file ) != NULL) &&
			 (*line_buf != '\0') )
		{
			if ( line_buf[ strlen( line_buf ) - 1 ] == '\n' )
				line_buf[ strlen( line_buf ) -1 ] = '\0';

			// Tokenize this line to get keywords
			keyword = strtok( line_buf, " ");
			if ( keyword != NULL ) {
				keyword_file = fopen( dst_file_path, "w" );
				if ( keyword_file == NULL ) {
					fprintf( stdout, "*** Error: could not open: %s\n",  dst_file_path );
					return( 1 );
				}

				// Write the keywords to the dovecot file
				do {
					if ( *keyword != '\0' )
						fprintf( keyword_file, "%u %s\n", i, keyword );

					i++;
				} while ( (keyword = strtok( NULL, " " ) ) != NULL );

				fclose( keyword_file );
			}
		}
		fclose( hdr_file );
	}

	return( 0 );

} // set_dovecot_keywords


// ------------------------------------------------------------------
//
// read_seen_file ()

int read_seen_file ( const char *in_seen_file )
{
	int		i			=- 1;
	char   *token		= NULL;
	FILE   *fp_seen		= NULL;
	char   *line_buf	= NULL;

	struct s_seen_data *out_seen_file;

	// Open the seen file
	if ( in_seen_file ) {
		fp_seen = fopen( in_seen_file, "r" );
		if ( fp_seen == NULL )
			return( 1 );
	}

	out_seen_file = malloc( sizeof(struct s_seen_data) );
	if ( out_seen_file == NULL ) {
		if ( g_debug != 0 )
			fprintf( stdout, "%s: memory allocation error\n",  __FUNCTION__ );

		fclose( fp_seen );
		return( 1 );
	}

	out_seen_file->seen_count = -1;
	out_seen_file->uid_flag = -1;

	while ( (line_buf = read_line( fp_seen )) != NULL ) {
		if ( strlen( line_buf ) )
			i++;
		else
			continue;

		// parse the cyrus seen file
		token = strtok( line_buf, " \t" );

		out_seen_file->seen_array[ i ].mbox_uid = cpy_str( token );

		token = strtok( NULL, " \t" );
		token = strtok( NULL, " \t" );
		token = strtok( NULL, " \t" );
		token = strtok( NULL, " \t" );
		token = strtok( NULL, " \t" );

		if ( token )
			out_seen_file->seen_array[i].seen_uids = cpy_str( token );
		else
			out_seen_file->seen_array[i].seen_uids = cpy_str( "" );

		free( line_buf );
		line_buf = NULL;
	}

	if ( i >= 0 )
		out_seen_file->seen_count = i + 1;

	fclose( fp_seen );

	g_seen_file = out_seen_file;

	return( 0 );

} // read_seen_file


// ------------------------------------------------------------------
//
// parse_seen_file ()

int parse_seen_file ( const char *in_mailbox, const unsigned long in_uidvalidity )
{
	int		i			= 0;
	int		index		= 0;
	char   *p			= NULL;
	size_t	str_len		= 0;
	char   *str			= NULL;
	char   *left_str	= NULL;
	char   *right_str	= NULL;
	char   *tuple_str	= NULL;
	char   *token_str	= NULL;

	char mbox		[ PATH_MAX + 1 ];
	char uniqueid	[ 17 ];

	if ( g_seen_file == NULL ) {
		if ( g_debug != 0 )
			fprintf( stdout, "No seen file found for %s\n", in_mailbox );

		return( 1 );
	}

	snprintf( mbox, PATH_MAX, "user.%s", in_mailbox );

	// Make mailbox uid
	mailbox_make_uniqueid( mbox, in_uidvalidity, uniqueid );

	// Find the seen line for this mailbox
	g_seen_file->uid_flag = -1;
	for ( i = 0; i < g_seen_file->seen_count; i++ ) {
		if ( strcmp( uniqueid, g_seen_file->seen_array[ i ].mbox_uid ) == 0 )
			g_seen_file->uid_flag = i;
	}

	if ( (g_seen_file->uid_flag) >= 0 && (g_seen_file->seen_count != -1) ) {
		if ( tuple_str = cpy_str( g_seen_file->seen_array[ g_seen_file->uid_flag ].seen_uids ) ) {
			// Scan across seen string parsing out seen uid's
			//	ie. 3:4,8:9,11:12,15:16,20  or 1,3,5,7,9,11

			// Comma separated token string
			token_str = strtok( tuple_str, "," );

			i = 0;
			while ( token_str ) {
				// Bail if we have exceeded max range
				assert( i < CYRUS_SEENMAX );
				
				str = cpy_str( token_str );

				// Look for a ':' separator
				p = strchr( str, ':' );
				index = -1;
				if ( p != NULL )
					index = p - str;

				// If ':' was not found
				if ( index == -1 ) {
					left_str = cpy_str( str );
					right_str = cpy_str( str );
				} else {
					// We found the ':', get uid min/max values
					if ( index >= strlen( str ) )
						left_str = cpy_str( str );
					else if ( index <= 0 )
						left_str = NULL;
					else {
						left_str = malloc( index + 1 );
						if ( left_str != NULL ) {
							memset( left_str, 0, (size_t)index + 1 );
							memcpy( left_str, str, (size_t)index );
						}
					}

					if ( index >= strlen( str ) )
						right_str = NULL;
					else if ( index < 0 )
						right_str = cpy_str( str );
					else {
						str_len = ( strlen( str ) - index - 1 );
						right_str = malloc( str_len + 1 );
						if ( right_str != NULL )
						{
							memset( right_str, 0, str_len + 1 );
							memcpy( right_str, str +(index + 1), str_len );

						}
					}
				}

				g_seen_file->uid_array[ i ].uid_min = atol( left_str );
				g_seen_file->uid_array[ i ].uid_max = atol( right_str );
				i++;

				// Free the strings
				if ( str ) {
					free( str );
					str = NULL;
				}

				if ( left_str ) {
					free( left_str );
					left_str = NULL;
				}

				if ( right_str ) {
					free( right_str );
					right_str = NULL;
				}

				// next token
				token_str = strtok( NULL, "," );
			}

			free( tuple_str );
			g_seen_file->uid_count = i;
		}
		else
			g_seen_file->uid_count = -1;
	}

	return( 0 );
} // parse_seen_file


// ------------------------------------------------------------------
//
// is_seen ()

int is_seen ( unsigned long in_uid )
{
	int i;

	if ( g_seen_file == NULL )
		return( 0 );

	if ( g_seen_file->uid_flag == -1 )
		return( 0 );

	for ( i = 0; i < g_seen_file->uid_count; i++ ) {
		if ( (in_uid >= g_seen_file->uid_array[ i ].uid_min) && (in_uid <= g_seen_file->uid_array[ i ].uid_max) )
			return( 1 );
	}
	return( 0 );
} // is_seen


// ------------------------------------------------------------------
//
// free_seen_file ()

void free_seen_file ( void )
{
	int i;

	if ( g_seen_file == NULL )
		return;

	for ( i = 0; i < g_seen_file->seen_count; i++ ) {
		free( g_seen_file->seen_array[ i ].mbox_uid );
		free( g_seen_file->seen_array[ i ].seen_uids );
	}
	free( g_seen_file );
} // free_seen_file


// ------------------------------------------------------------------
//
// read_line ()

char *read_line ( FILE *in_file )
{
	int		c			= 0;
	int		line_len	= -1;
	int		buf_size	= 1024;
	char   *out_buf		= NULL;

	if ( in_file && !feof( in_file ) ) {
		out_buf = (char *)malloc( buf_size );
		for( ;; ) {
			c = fgetc( in_file );
			line_len++;

			if ( line_len >= buf_size ) {
				buf_size += 4096;
				if ( g_debug != 0 )
					fprintf( stdout, "%s: realloc buffer: current size: %d -- new size: %d\n", __FUNCTION__, line_len, buf_size );

				out_buf = realloc( out_buf, buf_size );
				if ( out_buf == NULL )
					return( NULL );
			}
			out_buf[ line_len ] = (char)c;

			// Check for line termination or EOF
			if ( (c == '\n') || (c == '\r') || (c == '\0') || (c == EOF) ) {
				if( c == '\r' ) {
					c = fgetc( in_file );
					if ( c  !='\n' )
						ungetc(c, in_file);
				}
				// Terminate the string
				out_buf[ line_len ] = '\0';

				return( out_buf );
			}
		}
	}

	return( NULL );
} // read_line


// ------------------------------------------------------------------
//
// cpy_str ()

char * cpy_str ( const char *in_str )
{
	int		len			= 0;
	char	*out_str	= NULL;

	if ( in_str != NULL ) {
		len = strlen( in_str ) + 1;
		out_str = malloc( len );
		if ( out_str == NULL ) {
			if ( g_debug != 0 )
				fprintf( stdout, "%s: memory allocation error\n",  __FUNCTION__ );
		} else
			strlcpy( out_str, in_str,  len );
	}
	return( out_str );
} // cpy_str


// ------------------------------------------------------------------
//
// mailbox_make_uniqueid ()
//
//	Function from cyrus mailbox.c
//

void mailbox_make_uniqueid ( char *name, unsigned long in_uidvalidity, char *uniqueid )
{
	u_int32_t hash = 0;

	while (*name) {
		hash *= 251;
		hash += *name++;
		hash %= PRIME;
	}
	sprintf( uniqueid, "%08lx%08lx", (unsigned long)hash, in_uidvalidity );
} // mailbox_make_uniqueid


// ------------------------------------------------------------------
//

void rename_mailboxes ( const char *g_dest_dir )
{
	DIR				*p_dir		= NULL;
	struct dirent	*dir_entry;

	if ( p_dir = opendir( g_dest_dir ) ) {
		// Recursively scan across all mailboxes 
		while( (dir_entry = readdir(p_dir)) ) {
			// Skip the '.' & '..' directories
			if ( strcmp( dir_entry->d_name, "." ) &&  strcmp( dir_entry->d_name, "..") )
				map_mailbox_to_guid( g_dest_dir, dir_entry->d_name );
		}
		closedir( p_dir );
	}
} // rename_mailboxes


// ------------------------------------------------------------------
//

void set_attribute ( NSString *in_path )
{
	BOOL			is_dir		= NO;
	NSError			*nsError	= nil;
	NSArray			*nsArry		= nil;
	NSString		*nsStr_name	= nil;
	NSString		*nsStr_path	= nil;
	NSEnumerator	*nsEnum		= nil;
	NSFileManager	*nsFileMgr	= [NSFileManager defaultManager];
	NSDictionary	*nsDict_dir	= [NSDictionary dictionaryWithObjectsAndKeys:
									@"_dovecot", NSFileOwnerAccountName,
									@"mail", NSFileGroupOwnerAccountName,
									[NSNumber numberWithUnsignedLong:0700], NSFilePosixPermissions, nil];
	NSDictionary	*nsDict_file= [NSDictionary dictionaryWithObjectsAndKeys :
									@"_dovecot", NSFileOwnerAccountName,
									@"mail", NSFileGroupOwnerAccountName,
									[NSNumber numberWithUnsignedLong:0600], NSFilePosixPermissions, nil];

	if ( [nsFileMgr fileExistsAtPath: in_path isDirectory : &is_dir ] && is_dir ) {
		[nsFileMgr setAttributes: nsDict_dir ofItemAtPath: in_path error: &nsError];

		nsArry = [nsFileMgr contentsOfDirectoryAtPath: in_path error: &nsError];
		nsEnum = [nsArry objectEnumerator];
		while ( (nsStr_name = [nsEnum nextObject]) ) {
			nsStr_path = [in_path stringByAppendingPathComponent: nsStr_name];
			if ( [nsFileMgr fileExistsAtPath: nsStr_path isDirectory : &is_dir ] && is_dir )
				set_attribute( nsStr_path );
			else
				[nsFileMgr setAttributes: nsDict_file ofItemAtPath: nsStr_path error: &nsError];
		}
	}
	else
		[nsFileMgr setAttributes: nsDict_file ofItemAtPath: nsStr_path error: &nsError];
} // set_attribute


// ------------------------------------------------------------------

void fix_mailboxes ( const char *g_dest_dir )
{
	DIR				*p_dir		= NULL;
	NSString		*nsStr_path	= nil;
	struct dirent	*dir_entry;

	if ( p_dir = opendir( g_dest_dir ) ) {
		// Recursively scan across all mailboxes 
		while ( (dir_entry = readdir(p_dir)) ) {
			// Skip the '.' & '..' directories
			if ( strcmp( dir_entry->d_name, "." ) &&  strcmp( dir_entry->d_name, "..") ) {
				nsStr_path = [NSString stringWithFormat: @"%@/%@", [NSString stringWithUTF8String: g_dest_dir],
																  [NSString stringWithUTF8String: dir_entry->d_name]];
				set_attribute ( nsStr_path );
			}
		}
		closedir( p_dir );
	}
} // fix_mailboxes


// -----------------------------------------------------------------

void write_settings ( NSDictionary *in_dict )
{
	[in_dict writeToFile: MAIL_USER_SETTINGS_PLIST atomically: YES];

	NSDictionary		*nsDict_attrs	= [NSDictionary dictionaryWithObjectsAndKeys :
												@"_postfix", NSFileOwnerAccountName,
												@"mail", NSFileGroupOwnerAccountName,
												[NSNumber numberWithUnsignedLong: 0660], NSFilePosixPermissions,
												nil];

	[[NSFileManager defaultManager] setAttributes: nsDict_attrs ofItemAtPath: MAIL_USER_SETTINGS_PLIST error: nil];
} // set_attributes

// -----------------------------------------------------------------

void set_attributes ( NSString *in_path, NSString *in_owner, NSString *in_group, int in_perms )
{
	NSDictionary *nsDict_attrs	= [NSDictionary dictionaryWithObjectsAndKeys :
									in_owner, NSFileOwnerAccountName,
									in_group, NSFileGroupOwnerAccountName,
									[NSNumber numberWithUnsignedLong: in_perms], NSFilePosixPermissions,
									nil];

	[[NSFileManager defaultManager] setAttributes: nsDict_attrs ofItemAtPath: in_path error: nil];
} // set_attributes

// ------------------------------------------------------------------

void set_migration_flag ( const char *in_guid )
{
	NSFileManager	*nsFileMgr	= [NSFileManager defaultManager];

	if ( ![nsFileMgr fileExistsAtPath: MAIL_USER_SETTINGS_PLIST] ) {
		log_message( LOG_ERR, [NSString stringWithFormat: @"missing migration file: %@", MAIL_USER_SETTINGS_PLIST], nil );
		return;
	}

	// get the guid user list from mailusersettings.plist
	NSMutableDictionary	*users = [NSMutableDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( !users )
		users = [[[NSMutableDictionary alloc] init]autorelease];

	// look for the individual GUID and set migration flag and mail attribute
	NSMutableDictionary *user = [users objectForKey: [NSString stringWithUTF8String: in_guid]];
	if (user)
		[user setObject: kXMLValueAcctMigrated forKey: kXMLKeyMigrationFlag];
	else
		[users setObject: [NSMutableDictionary dictionaryWithObjectsAndKeys:
							kXMLValueAcctMigrated, kXMLKeyMigrationFlag, nil]
				forKey: [NSString stringWithUTF8String: in_guid]];

	// get mail attribute from od
	NSDictionary *mail_attribute = get_mail_attribute(in_guid);
	if (mail_attribute) {
		user = [users objectForKey: [NSString stringWithUTF8String: in_guid]];
		if (user)
			[user addEntriesFromDictionary: mail_attribute];
	}

	write_settings( users );
} // set_migration_flag


// ------------------------------------------------------------------
//

void set_migration_flags ( BOOL in_set )
{
	NSFileManager	*nsFileMgr	= [NSFileManager defaultManager];

	if ( ![nsFileMgr fileExistsAtPath: MAIL_USER_SETTINGS_PLIST] ) {
		log_message( LOG_ERR, [NSString stringWithFormat: @"missing migration file: %@", MAIL_USER_SETTINGS_PLIST], nil );
		return;
	}

	NSMutableDictionary	*users = [NSMutableDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( !users )
		return;

	NSMutableDictionary *user = nil;
	NSEnumerator *enumerator = [users objectEnumerator];
	while ( (user = [enumerator nextObject]) ) {
		if ( in_set )
			[user setObject: kXMLValueAcctMigrated forKey: kXMLKeyMigrationFlag];
		else
			[user setObject: kXMLValueAcctNotMigrated forKey: kXMLKeyMigrationFlag];
	}
	write_settings( users );
} // set_migration_flags


// ------------------------------------------------------------------
//

void fix_message_dates ( NSString *nsStr_in_path )
{
	BOOL			is_dir		= NO;
	NSError			*nsErr		= nil;
	NSArray			*nsArry		= nil;
	NSString		*nsStr_path	= nil;
	NSString		*nsStr_name	= nil;
	NSEnumerator	*nsEnum		= nil;
	NSFileManager	*nsFileMgr	= [NSFileManager defaultManager];

	if ( [nsFileMgr fileExistsAtPath: nsStr_in_path isDirectory: &is_dir] ) {
		if ( is_dir == YES ) {
			nsArry = [nsFileMgr contentsOfDirectoryAtPath: nsStr_in_path error: &nsErr];
			nsEnum = [nsArry objectEnumerator];
			while ( (nsStr_name = [nsEnum nextObject]) ) {
				nsStr_path = [nsStr_in_path stringByAppendingPathComponent: nsStr_name ];
				fix_message_dates( nsStr_path );
			}
		} else {
			NSMutableDictionary *nsMutDict = [NSMutableDictionary dictionaryWithDictionary:
													[nsFileMgr attributesOfItemAtPath: nsStr_in_path error: &nsErr]];
			if ( nsMutDict != nil ) {
				NSDate *nsDate = [nsMutDict objectForKey: @"NSFileModificationDate" ];
				if ( nsDate != nil ) {
					NSDate *nsDate_new = get_message_date( nsStr_in_path );
					if ( nsDate_new != nil ) {
						[nsMutDict setObject: nsDate_new forKey: @"NSFileModificationDate"];
						[nsFileMgr setAttributes: nsMutDict ofItemAtPath: nsStr_in_path error: &nsErr];
					}
				} 
			}
		}
	}
	nsArry = [nsFileMgr contentsOfDirectoryAtPath: nsStr_in_path error: &nsErr];
} // fix_message_dates


// ------------------------------------------------------------------
//

void map_mailbox_to_guid ( const char *in_path, const char *in_user )
{
	BOOL			is_dir				= NO;
	const char		*guid_map_str		= NULL;
	NSError			*nsError			= nil;
	NSString		*nsStr_user			= [NSString stringWithUTF8String: in_user];
	NSString		*nsStr_path			= [NSString stringWithUTF8String: in_path];
	NSString		*nsStr_guid			= nil;
	NSString		*nsStr_new_path		= nil;
	NSString		*nsStr_cur_path		= nil;
	NSFileManager	*nsFileMgr	= [NSFileManager defaultManager];

	nsStr_cur_path = [nsStr_path stringByAppendingPathComponent: nsStr_user];
	if ( [nsFileMgr fileExistsAtPath: nsStr_cur_path isDirectory : &is_dir ] && is_dir ) {
		if ( (guid_map_str = map_guid( in_user )) != NULL )
			nsStr_guid = [NSString stringWithUTF8String: guid_map_str];

		if ( nsStr_guid != nil ) {
			nsStr_new_path = [nsStr_path stringByAppendingPathComponent: nsStr_guid];
			if ( [ nsFileMgr moveItemAtPath: nsStr_cur_path toPath: nsStr_new_path error: &nsError ] == NO )
				fprintf( stdout, "Unable to rename path: %s to: %s\n",  [nsStr_cur_path UTF8String], [nsStr_new_path UTF8String] );
			else
				fprintf( stdout, "Mapping mailbox: %s to: %s\n",  [nsStr_cur_path UTF8String], [nsStr_new_path UTF8String] );
		} else
			fprintf( stdout, "No GUID found for: %s \n", in_user );
	} else
		fprintf( stdout, "Error: %s is missing or is not a directory\n",  [nsStr_cur_path UTF8String] );
} // map_mailbox_to_guid


// ------------------------------------------------------------------
//

const char *map_guid ( const char *in_user )
{
	ODQuery		*od_query			= nil;
	ODNode		*ds_search_node		= nil;
	ODRecord	*od_record			= nil;
	NSArray		*nsArray_values		= nil;
	NSArray		*nsArray_records	= nil;
	NSString	*nsStr_name			= nil;

	ds_search_node = [ODNode nodeWithSession: [ODSession defaultSession] type: kODNodeTypeAuthentication error: nil];
	if ( ds_search_node != nil ) {
		od_query = [ODQuery  queryWithNode: ds_search_node
							forRecordTypes: [NSArray arrayWithObject: @kDSStdRecordTypeUsers]
								 attribute: @kDSNAttrRecordName
								 matchType: kODMatchEqualTo
							   queryValues: [NSString stringWithUTF8String: in_user]
						  returnAttributes: [NSArray arrayWithObject: @kDS1AttrGeneratedUID]
							maximumResults: 1
									 error: nil];
		if ( od_query != nil ) {
			nsArray_records	= [od_query resultsAllowingPartial: NO error: nil];
			if ( (nsArray_records != nil) && [nsArray_records count] ) {
				od_record = [nsArray_records objectAtIndex: 0];
				if ( od_record != nil ) {
					// get the real name
					nsArray_values = [od_record valuesForAttribute: @kDS1AttrGeneratedUID error: nil];
					if ( nsArray_values != nil )
						nsStr_name = [nsArray_values objectAtIndex: 0];
				}
			}
		}
	}

	if ( (nsStr_name != nil) && [nsStr_name length] )
		return( [nsStr_name UTF8String] );

	return( NULL );
} // map_guid


// ------------------------------------------------------------------
//

const char *map_userid ( const char *in_guid )
{
	ODQuery		*od_query			= nil;
	ODNode		*ds_search_node		= nil;
	ODRecord	*od_record			= nil;
	NSArray		*nsArray_values		= nil;
	NSArray		*nsArray_records	= nil;
	NSString	*nsStr_name			= nil;

	ds_search_node = [ODNode nodeWithSession: [ODSession defaultSession] type: kODNodeTypeAuthentication error: nil];
	if ( ds_search_node != nil ) {
		od_query = [ODQuery  queryWithNode: ds_search_node
							forRecordTypes: [NSArray arrayWithObject: @kDSStdRecordTypeUsers]
								 attribute: @kDS1AttrGeneratedUID
								 matchType: kODMatchEqualTo
							   queryValues: [NSString stringWithUTF8String: in_guid]
						  returnAttributes: [NSArray arrayWithObject: @kDSNAttrRecordName]
							maximumResults: 1
									 error: nil];
		if ( od_query != nil ) {
			nsArray_records	= [od_query resultsAllowingPartial: NO error: nil];
			if ( (nsArray_records != nil) && [nsArray_records count] ) {
				od_record = [nsArray_records objectAtIndex: 0];
				if ( od_record != nil ) {
					// get the real name
					nsArray_values = [od_record valuesForAttribute: @kDSNAttrRecordName error: nil];
					if ( nsArray_values != nil )
						nsStr_name = [nsArray_values objectAtIndex: 0];
				}
			}
		}
	}

	if ( (nsStr_name != nil) && [nsStr_name length] )
		return( [nsStr_name UTF8String] );

	return( NULL );
} // map_userid

// ------------------------------------------------------------------
//

NSDictionary *get_mail_attribute ( const char *in_guid )
{
	NSDictionary *out_dict = nil;

	ODNode *ds_search_node = [ODNode nodeWithSession: [ODSession defaultSession] type: kODNodeTypeAuthentication error: nil];
	if ( ds_search_node == nil )
		return( NULL );

	ODQuery *od_query = [ODQuery  queryWithNode: ds_search_node
								 forRecordTypes: [NSArray arrayWithObject: @kDSStdRecordTypeUsers]
									  attribute: @kDS1AttrGeneratedUID
									  matchType: kODMatchEqualTo
								    queryValues: [NSString stringWithUTF8String: in_guid]
							   returnAttributes: [NSArray arrayWithObject: @kDS1AttrMailAttribute]
								 maximumResults: 1
										  error: nil];
	if ( od_query != nil ) {
		NSArray *records = [od_query resultsAllowingPartial: NO error: nil];
		if ( (records != nil) && [records count] ) {
			ODRecord *od_record = [records objectAtIndex: 0];
			if ( od_record != nil ) {
				// get the real name
				NSArray *values = [od_record valuesForAttribute: @kDS1AttrMailAttribute error: nil];
				if ( values && [values count] ) {
					NSData *data = [[values objectAtIndex: 0] dataUsingEncoding:NSUTF8StringEncoding];
					NSPropertyListFormat format;
					out_dict = [NSPropertyListSerialization propertyListFromData: data
								mutabilityOption: NSPropertyListImmutable format: &format errorDescription: nil];
				}
			}
		}
	}

	return( out_dict );
} // get_mail_attribute

// ------------------------------------------------------------------
//

void log_message ( int in_log_lvl, NSString *in_msg, NSError *in_ns_err )
{
	const char *cc_tag	= "Info:";

	if ( in_log_lvl == LOG_ERR )
		cc_tag = "Error:";
	else if ( in_log_lvl == LOG_WARNING )
		cc_tag = "Warning:";

	if ( (in_ns_err != nil) && ([[in_ns_err localizedFailureReason] length]) )
		syslog( in_log_lvl, "%s %s (%s)", cc_tag, [in_msg UTF8String], [[in_ns_err localizedFailureReason] UTF8String] );
	else
		syslog( in_log_lvl, "%s %s", cc_tag, [in_msg UTF8String] );
} // log_message


// ------------------------------------------------------------------
//

NSString *get_date_str ( NSString *nsStr_in_date )
{
	NSString	*nsStr			= nil;
	NSString	*nsStr_out		= nil;
	NSScanner	*nsScann_line	= nil;
	NSCharacterSet	*ws_charSet	= [NSCharacterSet whitespaceCharacterSet];

	nsStr = [NSString stringWithString: nsStr_in_date];

	while ( [nsStr rangeOfString: @";" options: NSLiteralSearch].location != NSNotFound ) {
		nsScann_line = [NSScanner scannerWithString: nsStr];
		[nsScann_line setCaseSensitive: YES];
		[nsScann_line setCharactersToBeSkipped: nil];

		if ( [nsScann_line scanUpToString: @";" intoString: nil ] )
		{
			[nsScann_line scanString: @";" intoString: nil ];
			[nsScann_line scanCharactersFromSet: ws_charSet intoString: nil];

			[nsScann_line scanUpToString: @"\r\n" intoString: &nsStr_out];
			nsStr = [NSString stringWithString: nsStr_out];
		}
	}
	return( nsStr_out );
} // 

// ------------------------------------------------------------------
//

NSDate *get_message_date ( NSString *nsStr_in_path )
{
	BOOL			hit				= NO;
	BOOL			done			= NO;
	NSDate			*nsDate_out		= nil;
	NSError			*nsErr			= nil;
	NSScanner		*nsScann_data	= nil;
	NSScanner		*nsScann_line	= nil;
	NSString		*nsStr_tmp		= nil;
	NSString		*nsStr_line		= nil;
	NSString		*nsStr_msg_data	= nil;
	NSString		*nsStr_date		= nil;
	NSDateFormatter *date_fmt		= [[NSDateFormatter alloc] init];
	NSMutableString	*sMutStr_header	= [NSMutableString stringWithString: @""];
	NSCharacterSet	*eol_charSet	= [NSCharacterSet characterSetWithCharactersInString: @"\r\n"];
	NSCharacterSet	*ws_charSet		= [NSCharacterSet whitespaceCharacterSet];

	nsStr_msg_data = [NSString stringWithContentsOfFile: nsStr_in_path encoding: NSUTF8StringEncoding error: &nsErr];
	if ( nsStr_msg_data == nil )
		return( nil );

	nsScann_data = [NSScanner scannerWithString: nsStr_msg_data];
	[nsScann_data setCaseSensitive: YES];
	[nsScann_data setCharactersToBeSkipped: nil];

	// Get first Received: header
	while ( ![nsScann_data isAtEnd] & !done )
	{
		[nsScann_data scanUpToCharactersFromSet: eol_charSet intoString: &nsStr_line];
		[nsScann_data scanCharactersFromSet: eol_charSet intoString: nil];

		if ( [nsStr_line hasPrefix: @"Received:"] && (hit == NO) ) {
			hit = YES;
			[sMutStr_header appendString: nsStr_line];
		} else if ( hit == YES ) {
			if ( [nsStr_line hasPrefix: @" "] || [nsStr_line hasPrefix: @"\t"] )
			{
				nsScann_line = [NSScanner scannerWithString: nsStr_line];
				[nsScann_line setCaseSensitive: YES];
				[nsScann_line setCharactersToBeSkipped: nil];

				[nsScann_line scanCharactersFromSet: ws_charSet intoString: nil];
				[nsScann_line scanUpToString: @"EOL" intoString: &nsStr_tmp];

				[sMutStr_header appendString: @" "];
				[sMutStr_header appendString: nsStr_tmp];
			} else {
				[sMutStr_header appendString: @"\r\n"];
				break;
			}
		}
	}

	if ( [sMutStr_header length] == 0 )
		return( nil );

	// parse Received: header and extract date

	nsStr_date = get_date_str( sMutStr_header );
	if ( nsStr_date == nil )
		return( nil );

	[date_fmt setFormatterBehavior:NSDateFormatterBehavior10_4];
	if ( [nsStr_date rangeOfString: @"(" options: NSLiteralSearch].location == NSNotFound )
		[date_fmt setDateFormat: @"EEE, dd MMM yyyy HH:m:ss vvvv"];
	else if ( isdigit([nsStr_date characterAtIndex: 1]) )
		[date_fmt setDateFormat: @"dd MMM yyyy HH:m:ss vvvv (zzz)"];
	else
		[date_fmt setDateFormat: @"EEE, dd MMM yyyy HH:m:ss vvvv (zzz)"];

	nsDate_out = [date_fmt dateFromString: nsStr_date];

	return( nsDate_out );
	
} // get_message_date

// ------------------------------------------------------------------

NSDictionary *get_alt_data_stores ( int in_print )
{
	NSError *ns_err = nil;
	NSMutableDictionary	*out_dict = [[[NSMutableDictionary alloc] init] autorelease];

	if ( in_print ) {
		printf("\n" );
		printf("alternate data store locations and tags\n" );
		printf("---------------------------------------\n" );
	}

	NSString *file_data = [NSMutableString stringWithContentsOfFile: @"/etc/dovecot/partition_map.conf" encoding: NSUTF8StringEncoding error: &ns_err];
	if ( file_data && [file_data length] ) {
		NSString *map_str = nil;
		file_data = [file_data stringByTrimmingCharactersInSet: [NSCharacterSet whitespaceAndNewlineCharacterSet]];
		NSArray *alt_stores = [file_data componentsSeparatedByCharactersInSet: [NSCharacterSet newlineCharacterSet]];
		NSEnumerator *ns_enum = [alt_stores objectEnumerator];
		while ( (map_str = [ns_enum nextObject]) ) {
			NSArray *mapping = [map_str componentsSeparatedByCharactersInSet: [NSCharacterSet characterSetWithCharactersInString: @":"]];
			[out_dict setObject: [mapping objectAtIndex: 1] forKey: [mapping objectAtIndex: 0]];
			if ( in_print )
				printf("tag:%s  path:%s \n", [[mapping objectAtIndex: 0]UTF8String], [[mapping objectAtIndex: 1]UTF8String] );
		}
	}

	return( out_dict );
} // get_alt_data_stores

// ------------------------------------------------------------------

void set_alt_data_store_tag ( const char *in_tag, const char *in_path )
{
	BOOL is_set = NO;
	BOOL is_dir = NO;
	NSDictionary *alt_stores = get_alt_data_stores(0);
	NSMutableDictionary *maps = [[alt_stores mutableCopy] autorelease];
	NSMutableString *new_map = [[[NSMutableString alloc]init ]autorelease];
	NSString *path_str = [NSString stringWithCString: in_path encoding: NSUTF8StringEncoding];
	if ( [[NSFileManager defaultManager] fileExistsAtPath: path_str isDirectory: &is_dir ] && is_dir ) {
		NSString *tag_str = [NSString stringWithCString: in_tag encoding: NSUTF8StringEncoding];
		// change existing tag path value
		if ( [maps objectForKey: tag_str] ) {
			[maps setObject: path_str forKey: tag_str];
			is_set = YES;
		}

		// sanity check, make sure "default" exists, otherwise create it
		if ( ![maps objectForKey: @"default"] )
			[maps setObject: DEFAULT_DATA_LOCATION forKey: @"default"];
			
		[new_map appendString: [NSString stringWithFormat: @"default:%@\n", [maps objectForKey: @"default"]]];
		id key = 0;
		NSEnumerator *ns_enum = [[maps allKeys] objectEnumerator];
		while ( (key = [ns_enum nextObject]) ) {
			if ( ![key isEqualToString: @"default"] )
				[new_map appendString: [NSString stringWithFormat: @"%@:%@\n", key, [maps objectForKey: key]]];
		}
		if ( !is_set )
			[new_map appendString: [NSString stringWithFormat: @"%@:%@\n", tag_str, path_str]];
		set_attributes ( path_str, @"_dovecot", @"mail", 0775 );
		[new_map writeToFile: DOVECOT_PARTITION_MAPS atomically: YES encoding: NSUTF8StringEncoding error: nil];
		set_attributes ( DOVECOT_PARTITION_MAPS, @"root", @"wheel", 0644 );
		get_alt_data_stores(1);
	} else
		printf("Error: path: %s does not exist\n", in_path );
} // set_alt_data_store_tag

// ------------------------------------------------------------------

void reset_alt_data_store_tag ( const char *in_tag )
{
	NSString *tag_str = [NSString stringWithCString: in_tag encoding: NSUTF8StringEncoding];
	if ( [tag_str isEqualToString:  @"default"] ) {
		printf("Cannot reset default alternate store tag\n");
		return;
	}

	NSDictionary *maps = get_alt_data_stores(0);
	NSMutableString *new_map = [[[NSMutableString alloc]init ]autorelease];
	if ( [maps objectForKey: tag_str] ) {
		id key = 0;
		NSEnumerator *ns_enum = [[maps allKeys] objectEnumerator];
		while ( (key = [ns_enum nextObject]) ) {
			if ( ![key isEqualToString: tag_str] )
				[new_map appendString: [NSString stringWithFormat: @"%@:%@\n", key, [maps objectForKey: key]]];
		}
		[new_map writeToFile: DOVECOT_PARTITION_MAPS atomically: YES encoding: NSUTF8StringEncoding error: nil];
		set_attributes ( DOVECOT_PARTITION_MAPS, @"root", @"wheel", 0644 );
	} else
		printf("Error: tag does not exist: %s\n", in_tag );

	get_alt_data_stores(1);
} // reset_alt_data_store_tag

// ------------------------------------------------------------------

void list_alt_data_stores( const char *in_guid )
{
	id key = 0;

	printf("\n" );
	printf("local user alternate data store location settings\n" );
	printf("-------------------------------------------------\n" );

	NSDictionary *users_dict = [NSDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( users_dict ) {
		NSDictionary *maps = get_alt_data_stores(0);
		if ( in_guid ) {
			NSString *guid = [NSString stringWithCString: in_guid encoding: NSUTF8StringEncoding];
			NSDictionary *user_dict = [users_dict objectForKey: guid];
			if ( !user_dict )
				printf("user: default:%s tag:default path:%s\n", map_userid([key UTF8String]), [[maps objectForKey: @"default"]UTF8String]);
			else {
				NSString *alt_tag = [user_dict objectForKey: @"kAltMailStoreLoc"];
				if ( [maps objectForKey: alt_tag] )
					printf("user:%s store  tag:%s  path:%s\n", map_userid(in_guid), [alt_tag UTF8String], [[maps objectForKey: alt_tag]UTF8String]);
				else
					printf("user:%s store  tag:default  path:%s\n", map_userid(in_guid), [[maps objectForKey: @"default"]UTF8String]);
			}
		} else {
			NSEnumerator *enumer = [[users_dict allKeys] objectEnumerator];
			while ( (key = [enumer nextObject]) ) {
				NSDictionary *user_dict = [users_dict objectForKey: key];
				NSString *alt_tag = [user_dict objectForKey: @"kAltMailStoreLoc"];
				if ( [maps objectForKey: alt_tag] )
					printf("user:%s store  tag:%s  path:%s\n", map_userid([key UTF8String]), [alt_tag UTF8String], [[maps objectForKey: alt_tag]UTF8String]);
				else
					printf("user:%s store  tag:default  path:%s\n", map_userid([key UTF8String]), [[maps objectForKey: @"default"]UTF8String]);
			}
		}
	}
	printf("\n" );
} // list_alt_data_stores

// ------------------------------------------------------------------

void set_alt_data_store ( const char *in_guid, const char *in_store_tag )
{
	NSMutableDictionary *users_dict = [NSMutableDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( !users_dict )
		users_dict = [[[NSMutableDictionary alloc] init] autorelease];

	NSString *guid = [NSString stringWithCString: in_guid encoding: NSUTF8StringEncoding];
	NSString *tag = [NSString stringWithCString: in_store_tag encoding: NSUTF8StringEncoding];
	NSDictionary *maps = get_alt_data_stores(0);

	NSMutableDictionary *user_dict = [users_dict objectForKey: guid];
	if ( user_dict ) {
		if ( [maps objectForKey: tag] )
			[user_dict setObject: tag forKey: @"kAltMailStoreLoc"];
		else
			[user_dict setObject: @"default" forKey: @"kAltMailStoreLoc"];
	} else {
		if ( [maps objectForKey: tag] )
			[users_dict setObject: [NSDictionary dictionaryWithObjectsAndKeys:
									tag, @"kAltMailStoreLoc", nil] forKey: guid];
		else
			[users_dict setObject: [NSDictionary dictionaryWithObjectsAndKeys:
									@"default", @"kAltMailStoreLoc", nil] forKey: guid];
	}
	write_settings( users_dict );

	list_alt_data_stores( in_guid );
} // set_alt_data_store

// ------------------------------------------------------------------

void reset_alt_data_stores ( const char *in_guid )
{
	NSMutableDictionary *users_dict = [NSMutableDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( !users_dict )
		users_dict = [[[NSMutableDictionary alloc] init] autorelease];

	NSString *guid = [NSString stringWithCString: in_guid encoding: NSUTF8StringEncoding];

	NSMutableDictionary *user_dict = [users_dict objectForKey: guid];
	if ( user_dict ) {
		[user_dict setObject: @"default" forKey: @"kAltMailStoreLoc"];
	} else {
		[users_dict setObject: [NSDictionary dictionaryWithObjectsAndKeys:
								@"default", @"kAltMailStoreLoc", nil] forKey: guid];
	}
	write_settings( users_dict );

	list_alt_data_stores( in_guid );
} // reset_alt_data_stores

// ------------------------------------------------------------------

void set_auto_forward( const char *in_guid, const char *in_fwd_addr )
{
	NSMutableDictionary *users_dict = [NSMutableDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( !users_dict )
		users_dict = [[[NSMutableDictionary alloc] init] autorelease];

	NSString *guid = [NSString stringWithCString: in_guid encoding: NSUTF8StringEncoding];
	NSString *fwd_addr = [NSString stringWithCString: in_fwd_addr encoding: NSUTF8StringEncoding];
	NSMutableDictionary *user_dict = [users_dict objectForKey: guid];
	if ( user_dict ) {
		[user_dict setObject: @"Forward" forKey: @"kMailAccountState"];
		[user_dict setObject: fwd_addr forKey: @"kAutoForwardValue"];
	} else {
		[users_dict setObject: [NSDictionary dictionaryWithObjectsAndKeys:
									@"Forward", @"kMailAccountState",
									fwd_addr, @"kAutoForwardValue", nil] forKey: guid];
	}
	write_settings( users_dict );

	list_auto_forwards( in_guid );
} //  set_auto_forward


// ------------------------------------------------------------------

void reset_auto_forward( const char *in_guid )
{
	NSMutableDictionary *users_dict = [NSMutableDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( !users_dict )
		return;

	NSString *guid = [NSString stringWithCString: in_guid encoding: NSUTF8StringEncoding];
	NSMutableDictionary *user_dict = [users_dict objectForKey: guid];
	if ( user_dict ) {
		[user_dict setObject: @"" forKey: @"kMailAccountState"];
		[user_dict setObject: @"" forKey: @"kAutoForwardValue"];
	}
	write_settings( users_dict );

	list_auto_forwards( in_guid );
} //  reset_auto_forward


// ------------------------------------------------------------------

void list_auto_forwards ( const char *in_guid )
{
	id key = 0;

	printf("\n" );
	printf("local user auto-forward settings\n" );
	printf("--------------------------------\n" );

	NSDictionary *users_dict = [NSDictionary dictionaryWithContentsOfFile: MAIL_USER_SETTINGS_PLIST];
	if ( users_dict ) {
		if ( in_guid ) {
			NSString *guid = [NSString stringWithCString: in_guid encoding: NSUTF8StringEncoding];
			NSDictionary *user_dict = [users_dict objectForKey: guid];
			if ( [[user_dict objectForKey: @"kMailAccountState"] isEqualToString: @"Forward"] )
				printf("user: %s: <%s>\n", map_userid(in_guid), [[user_dict objectForKey: @"kAutoForwardValue"]UTF8String]);
		} else {
			NSEnumerator *enumer = [[users_dict allKeys] objectEnumerator];
			while ( (key = [enumer nextObject]) ) {
				NSDictionary *user_dict = [users_dict objectForKey: key];
				if ( [[user_dict objectForKey: @"kMailAccountState"] isEqualToString: @"Forward"] )
					printf("user: %s: <%s>\n", map_userid([key UTF8String]), [[user_dict objectForKey: @"kAutoForwardValue"]UTF8String]);
			}
		}
	}
	printf("\n" );
} // list_auto_forwards
