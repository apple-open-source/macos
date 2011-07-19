/*
 *  Contains: Header definitions for Cyrus to Dovecot maildir mail migration
 *	Written by: Michale Dasenbrock
 *  Copyright:  © 2008-2011 Apple Inc., All rights reserved.
 *
 */

#ifndef __CVT_MAIL_H__
#define	__CVT_MAIL_H__

// ------------------------------------------------------------------

#define BUF_SIZE		4096

#define CYRUS_FOLDERMAX	4096
#define CYRUS_SEENMAX	64000

// ------------------------------------------------------------------
// Definitions from cyrus mailbox.h
//	Needed for parsing the index and header files

#define PRIME (2147484043UL)

#define MAX_USER_FLAGS (16*8)

#define FLAG_ANSWERED (1<<0)
#define FLAG_FLAGGED (1<<1)
#define FLAG_DELETED (1<<2)
#define FLAG_DRAFT (1<<3)

#define MAILBOX_HEADER_MAGIC ("\241\002\213\015Cyrus mailbox header\n" \
     "\"The best thing about this system was that it had lots of goals.\"\n" \
     "\t--Jim Morris on Andrew\n")

#define	DEFAULT_DATA_LOCATION		@"/Library/Server/Mail/Data/mail"
#define	DOVECOT_PARTITION_MAPS		@"/etc/dovecot/partition_map.conf"
#define	MAIL_MIGRATION_PLIST		@"/var/db/.mailmigration.plist"
#define	MAIL_USER_SETTINGS_PLIST	@"/var/db/.mailusersettings.plist"

/* Migration key/values */
#define	kXMLKeyMigrationFlag			@"kMigrationFlag"
	#define	kXMLValueAcctMigrated			@"AcctMigrated"
	#define	kXMLValueAcctNotMigrated		@"AcctNotMigrated"

struct index_header {
	uint32_t	generation_no;
	uint32_t	format;
	uint32_t	minor_version;

	uint32_t	start_offset;
	uint32_t	record_size;
	uint32_t	exists;
	uint32_t	last_appenddate;
	uint32_t	last_uid;
	uint32_t	quota_mailbox_used64;
	uint32_t	quota_mailbox_used;
	uint32_t	pop3_last_login;
	uint32_t	uidvalidity;

	uint32_t	deleted;
	uint32_t	answered;
	uint32_t	flagged;

	uint32_t	pop3_new_uidl_or_mailbox_options;
	uint32_t	leaked_cache;
	uint32_t	highestmodseq64;
	uint32_t	highestmodseq;
	uint32_t	spare0;
	uint32_t	spare1;
	uint32_t	spare2;
	uint32_t	spare3;
	uint32_t	spare4;
};

struct index_entry {
	uint32_t	uid;
	uint32_t	internaldate;
	uint32_t	sentdate;
	uint32_t	size;
	uint32_t	header_size;
	uint32_t	content_offset;
	uint32_t	cache_offset;
	uint32_t	last_updated;
	uint32_t	system_flags;
	uint32_t	user_flags[ MAX_USER_FLAGS / 32 ];
	uint32_t	content_lines;
	uint32_t	cache_version;
	uint8_t		message_uuid[ 12 ];
	uint32_t	modseq64;
	uint32_t	modseq;
};

// ------------------------------------------------------------------
//	Local structs

struct s_seen_uids {
	unsigned long uid_min;
	unsigned long uid_max;
};

struct s_seen_line {
	char	*mbox_uid;
	char	*seen_uids;
};

struct s_seen_data
{
	int seen_count;
	int uid_flag;
	int uid_count;
	struct s_seen_line seen_array[ CYRUS_FOLDERMAX + 1 ];
	struct s_seen_uids uid_array[ CYRUS_SEENMAX + 1 ];
};

// ------------------------------------------------------------------

void	usage				( int in_exit_code );
int		verify_path			( const char *in_path );
int		scan_account		( char *in_cy_spool_dir, char *in_dst_root, char *in_acct_dir );
int		set_quota			( char *in_dst_root, char *in_dir );
int		set_subscribe		( char *in_dst_root, char *in_dir );
int		create_maildir_dirs	( char *root, char *dir, int is_root );
int		migrate_mail		( char *in_src_path, char *dest, int is_root );
int		migrate_message		( char *in_src, unsigned long in_size, char *dst_a, char *in_flags, unsigned long *out_size );
int		read_seen_file		( const char *in_seen_file );
int		parse_seen_file		( const char *in_mailbox, const unsigned long in_uidvalidity );
int		is_seen				( unsigned long in_uid );
void	free_seen_file		( void );

// ------------------------------------------------------------------
// Utility functions

char   *cpy_str				( const char *in_str );
char   *read_line			( FILE *in_file );

// ------------------------------------------------------------------
// Prototype from cyrus mailbox.h

void	mailbox_make_uniqueid	( char *name, unsigned long uidvalidity, char *uniqueid );


#endif // __CVT_MAIL_H__
