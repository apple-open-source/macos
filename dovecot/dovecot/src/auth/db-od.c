/*
 * Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

#include "auth-common.h"

#if defined (USERDB_OD) || defined(PASSDB_OD)

#include "userdb.h"
#include "db-od.h"

#include "buffer.h"
#include "istream.h"
#include "hash.h"
#include "str.h"
#include "var-expand.h"
#include "dtrace.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <membership.h>
#include <membershipPriv.h>

#include <CoreDaemon/CoreDaemon.h>

XSEventPortRef	gEventPort = NULL;

int				od_pos_cache_ttl	= 3600;
int				od_neg_cache_ttl	= 60;
bool			od_use_getpwnam_ext	= TRUE;
const char		*def_path			= "/Library/Server/Mail/Data/mail";
const char		*users_path			= "/var/db/.mailusersettings.plist";
const char		*notify_path		= "/etc/dovecot/notify/notify.plist";
static bool		mail_sacl_enabled	= FALSE;
static time_t	sacl_check_delta	= 0;

static struct db_od *ods;

static bool		od_open				( struct db_od *in_od_info );
static void		od_get_acct_state	( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict );
static void		od_get_auto_forward	( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict );
static void		od_get_imap_login	( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict );
static void		od_get_pop3_login	( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict );
static void		od_get_migration	( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec );
static void		od_get_acct_loc		( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec );
static void		od_get_alt_loc		( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec );
static void		od_get_mail_quota	( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec );
static struct	od_user *od_get_user	( struct auth_request *in_request, struct db_od *in_od_info, const char *in_user_name );
static bool		od_get_attributes_local ( struct auth_request *in_request, struct od_user *out_user_rec, const char *in_user_guid );
static void		od_set_attributes_local ( struct auth_request *in_request, CFMutableDictionaryRef in_user_dict, struct od_user *in_user_rec );


/* ------------------------------------------------------------------
 * send server events
 * ------------------------------------------------------------------*/
 
void send_server_event ( struct auth_request *in_request, const od_auth_event_t in_event_code, const char *in_user, const char *in_addr )
{
	CFTypeRef keys[3];
	CFTypeRef values[3];
	CFStringRef cfstr_event = NULL;

	if (in_user == NULL || in_addr == NULL)
		return;

	/* create a port to the event server */
	if ( gEventPort == NULL )
		gEventPort = XSEventPortCreate(nil);

	keys[0] = CFSTR("eventType");
	keys[1] = CFSTR("clientIP");
	keys[2] = CFSTR("user_name");

	/* set event code string */
	switch ( in_event_code ) {
		case OD_AUTH_SUCCESS:
			cfstr_event = CFStringCreateWithCString(NULL, "auth.success", kCFStringEncodingMacRoman);
			break;
		case OD_AUTH_FAILURE:
			cfstr_event = CFStringCreateWithCString(NULL, "auth.failure", kCFStringEncodingMacRoman);
			break;
		default:
			auth_request_log_debug(in_request, "od", "unknown sever event: %d", in_event_code);
			return;
	}

	CFStringRef cfstr_addr = CFStringCreateWithCString(NULL, in_addr, kCFStringEncodingMacRoman);
	CFStringRef cfstr_user = CFStringCreateWithCString(NULL, in_user, kCFStringEncodingMacRoman);

	values[0] = cfstr_event;
	values[1] = cfstr_addr;
	values[2] = cfstr_user;

     CFDictionaryRef dict_event = CFDictionaryCreate(NULL, keys, values, 
                                               sizeof(keys) / sizeof(keys[0]), 
                                               &kCFTypeDictionaryKeyCallBacks, 
                                               &kCFTypeDictionaryValueCallBacks); 
	
	/* send the event */
	(void)XSEventPortPostEvent(gEventPort, cfstr_event, dict_event);

	CFRelease(cfstr_addr);
	CFRelease(cfstr_user);
	CFRelease(cfstr_event);
	CFRelease(dict_event);
} /* send_server_event */

/* ------------------------------------------------------------------
 *	close_server_event_port ()
 * ------------------------------------------------------------------*/

void close_server_event_port ( void )
{
	if ( gEventPort != NULL )
		XSEventPortDelete(gEventPort);
} /* close_server_event_port */

/* ------------------------------------------------------------------
 *	db_od_print_cf_error ()
 *
 *		print error returned in CFErrorRef
 * ------------------------------------------------------------------*/

void db_od_print_cf_error ( struct auth_request *in_request, CFErrorRef in_cf_err_ref, const char *in_default_str )
{
	if ( in_cf_err_ref ) {
		CFStringRef cf_str_ref = CFErrorCopyFailureReason( in_cf_err_ref );
		if ( cf_str_ref ) {
			char *c_str = (char*)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
			if (in_request)
				auth_request_log_error(in_request, "od", "%s", c_str);
			else
				i_error( "od: %s", c_str );

			CFRelease(cf_str_ref);
			return;
		}
	}

	if (in_request)
		auth_request_log_error(in_request, "od", "%s", in_default_str);
	else
		i_error( "od: %s", in_default_str );
} /* db_od_print_cf_error */


/* ------------------------------------------------------------------
 *	db_od_user_unref ()
 * ------------------------------------------------------------------ */

void db_od_user_unref ( struct od_user **_in_user_info )
{
	struct od_user *in_user_info = *_in_user_info;

	*_in_user_info = NULL;

	i_assert(in_user_info->refcount >= 1);
	if (--in_user_info->refcount <= 0) {
		i_free(in_user_info->user_guid);
		i_free(in_user_info->record_name);
		i_free(in_user_info->acct_loc);
		i_free(in_user_info->alt_data_loc);
		i_free(in_user_info);
	}
} /* db_od_user_unref */


/* ------------------------------------------------------------------
 *	od_add_user ()
 * ------------------------------------------------------------------*/

static void od_add_user ( struct auth_request *in_request, struct db_od *in_od_info,
							struct od_user *in_od_user, const char *in_user_name )
{
	if ( (in_od_user == NULL) || (in_user_name == NULL) ) {
		auth_request_log_error(in_request, "od", "unable to add user to table: Null user id" );
		return;
	}

	i_assert(hash_table_lookup(in_od_info->users_table, in_user_name) == NULL);

	auth_request_log_debug(in_request, "od", "caching user %s as %s", in_od_user->record_name, in_user_name );

	++in_od_user->refcount;
	hash_table_insert( in_od_info->users_table, i_strdup(in_user_name), in_od_user );

} /* od_add_user */


/* ------------------------------------------------------------------
 *	od_open ()
 * ------------------------------------------------------------------ */

static bool od_open ( struct db_od *in_od_info )
{
	CFErrorRef	cf_err_ref = NULL;

	// get session ref
	if ( in_od_info->od_session_ref )
		CFRelease( in_od_info->od_session_ref );

	in_od_info->od_session_ref = ODSessionCreate( kCFAllocatorDefault, NULL, &cf_err_ref );
	if ( !in_od_info->od_session_ref ) {
		db_od_print_cf_error( NULL, cf_err_ref, "Unable to create OD Session" );
		if ( cf_err_ref )
			CFRelease( cf_err_ref );
		return( FALSE );
	}

	// get seach node
	if ( in_od_info->od_node_ref )
		CFRelease( in_od_info->od_node_ref );

	in_od_info->od_node_ref = ODNodeCreateWithNodeType( kCFAllocatorDefault, in_od_info->od_session_ref, kODNodeTypeAuthentication, &cf_err_ref );
	if ( !in_od_info->od_node_ref ) {
		db_od_print_cf_error( NULL, cf_err_ref, "Unable to create OD Node Reference" );
		CFRelease( in_od_info->od_session_ref );
		in_od_info->od_session_ref = NULL;
		if ( cf_err_ref )
			CFRelease( cf_err_ref );
		return( FALSE );
	}

	in_od_info->users_table = hash_table_create( system_pool, system_pool, 100, str_hash, (hash_cmp_callback_t *)strcmp);
	return( TRUE );
} /* od_open */


/* ------------------------------------------------------------------
 *	db_od_init ()
 * ------------------------------------------------------------------*/

struct db_od * db_od_init ( bool in_userdb )
{
	struct db_od	*out_db = ods;

	if ( out_db != NULL ) {
		out_db->refcount++;
		out_db->userdb = TRUE;
		return( out_db );
	}

	out_db = i_new( struct db_od, 1 );
	out_db->refcount = 1;
	out_db->userdb = in_userdb;
	out_db->pos_cache_ttl = od_pos_cache_ttl;
	out_db->neg_cache_ttl = od_neg_cache_ttl;
	out_db->use_getpwnam_ext = od_use_getpwnam_ext;

	ods = out_db;

	return( out_db );
} /* db_od_init */


/* ------------------------------------------------------------------
 *	db_od_do_init ()
 * ------------------------------------------------------------------*/

void db_od_do_init ( struct db_od *in_od_info )
{
	i_assert( in_od_info != NULL );
	if ( od_open( in_od_info ) == FALSE )
		exit( FATAL_DEFAULT );
} /* db_od_do_init */


/* ------------------------------------------------------------------
 *	db_od_unref ()
 * ------------------------------------------------------------------*/

void db_od_unref ( struct db_od **in_od_info_p )
{
	struct db_od *in_od_info = *in_od_info_p;

	*in_od_info_p = NULL;
	i_assert( in_od_info->refcount >= 0 );
	if ( --in_od_info->refcount > 0 )
		return;

	if ( in_od_info->od_node_ref != NULL ) {
		CFRelease( in_od_info->od_node_ref );
		in_od_info->od_node_ref = NULL;
	}

	if ( in_od_info->od_session_ref != NULL ) {
		CFRelease( in_od_info->od_session_ref );
		in_od_info->od_session_ref = NULL;
	}

	if ( in_od_info->users_table != NULL ) {
		struct hash_iterate_context *iter = hash_table_iterate_init(in_od_info->users_table);
		char *name;
		struct od_user *user;

		while (hash_table_iterate(iter, (void **) &name, (void **) &user)) {
			hash_table_remove(in_od_info->users_table, name);
			i_free(name);
			db_od_user_unref(&user);
		}
		hash_table_iterate_deinit(&iter);
		hash_table_destroy( &in_od_info->users_table );
	}

	i_free( in_od_info );
} /* db_od_unref */


/* ------------------------------------------------------------------
 *	db_od_get_ms_path ()
 *		- get mail store path, default or alt location
 * ------------------------------------------------------------------*/

const char *db_od_get_ms_path ( struct auth_request *in_request, const char *in_tag, const char *in_partition_map )
{
	const char *const	*map_str		= NULL;
	const char			*partition_tag	= NULL;
	const char			*out_path		= NULL;
	const char			*dpartpath		= def_path;

	/* If there are defined partitios, then we better have a partition map */
	if ( in_partition_map == NULL ) {
		auth_request_log_error(in_request, "od", "missing partition map file: using default mail location: %s", def_path );
		return( def_path );
	}

	/* Set the tag to include a :, ie. default:  */
	partition_tag = t_strconcat( in_tag, ":", NULL );

	/* We've already read in partition map file, look for a match  */
	/*  ie.  default:/var/spool/306/dovecot/mail  */
	/*       partition1:/var/spool/spare  */

	for ( map_str = t_strsplit( in_partition_map, "\n"); *map_str != NULL; map_str++ ) {
		if (dpartpath == def_path && strncmp(*map_str, "default:", 8) == 0)
			dpartpath = t_strdup(*map_str + 8);
		if ( strncmp( partition_tag, *map_str, strlen(partition_tag)) == 0 ) {
			out_path = t_strdup_noconst( *map_str + strlen(partition_tag) );
			break;
		}
	}

	if ( out_path == NULL ) {
		auth_request_log_error( in_request, "od", "unable to find partition for tag=%s: using default mail partition: %s", in_tag, dpartpath );
		return( dpartpath );
	}

	return( out_path );
} /* db_od_get_ms_path */


/* ------------------------------------------------------------------
 *	od_get_acct_state ()
 * ------------------------------------------------------------------*/

static void od_get_acct_state ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict )
{
	if ( CFDictionaryContainsKey( in_dict, CFSTR(kXMLKeyAcctState) ) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR( kXMLKeyAcctState ) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			if (out_user_dict != NULL)
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyAcctState), cf_str_ref);
		}
	}
} /* od_get_acct_state */


/* ------------------------------------------------------------------
 *	od_get_auto_forward ()
 * ------------------------------------------------------------------*/

static void od_get_auto_forward ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict )
{
	if ( CFDictionaryContainsKey( in_dict, CFSTR(kXMLKeyAutoFwd) ) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR(kXMLKeyAutoFwd) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			if (out_user_dict != NULL)
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyAutoFwd), cf_str_ref);
		}
	}
} /* od_get_auto_forward */


/* ------------------------------------------------------------------
 *	od_get_migration ()
 * ------------------------------------------------------------------*/

static void od_get_migration ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec )
{
	if ( CFDictionaryContainsKey( in_dict, CFSTR(kXMLKeyMigrationFlag) ) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR(kXMLKeyMigrationFlag) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			char *c_str = (char*)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
			if ( c_str && strlen(c_str) && !strcmp(c_str, kXMLValueAcctMigrated))
				out_user_rec->acct_state |= acct_migrated;

		if (out_user_dict != NULL)
			CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyMigrationFlag), CFSTR(kXMLValueAcctNotMigrated));
		}
	}
} /* od_get_migration */


/* ------------------------------------------------------------------
 *	od_get_imap_login ()
 * ------------------------------------------------------------------*/

static void od_get_imap_login ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict )
{
	if ( CFDictionaryContainsKey( in_dict, CFSTR(kXMLKeykIMAPLoginState) ) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR( kXMLKeykIMAPLoginState ) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			if (out_user_dict != NULL)
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeykIMAPLoginState), cf_str_ref);
		}
	}
} /* od_get_imap_login */


/* ------------------------------------------------------------------
 *	od_get_pop3_login ()
 * ------------------------------------------------------------------*/

static void od_get_pop3_login ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict )
{
	if ( CFDictionaryContainsKey( in_dict, CFSTR(kXMLKeyPOP3LoginState) ) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR(kXMLKeyPOP3LoginState) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			if (out_user_dict != NULL)
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyPOP3LoginState), cf_str_ref);
		}
	}
} /* od_get_pop3_login */


/* ------------------------------------------------------------------
 *	od_get_acct_loc ()
 * ------------------------------------------------------------------*/

static void od_get_acct_loc ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec )
{
	/* don't leak previous value (if any) */
	if ( out_user_rec->acct_loc ) {
		i_free( out_user_rec->acct_loc );
		out_user_rec->acct_loc = NULL;
	}

	if ( CFDictionaryContainsKey( in_dict, CFSTR( kXMLKeyAcctLoc ) ) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR( kXMLKeyAcctLoc ) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			char *c_str = (char*)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
			if ( c_str && strlen(c_str) )
				out_user_rec->acct_loc = i_strdup(c_str);

			if (out_user_dict != NULL)
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyAcctLoc), cf_str_ref);
		}
	}
} /* od_get_acct_loc */


/* ------------------------------------------------------------------
 *	od_get_alt_loc ()
 * ------------------------------------------------------------------*/

static void od_get_alt_loc ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec )
{
	/* Default value */
	if ( out_user_rec->alt_data_loc ) {
		i_free( out_user_rec->alt_data_loc );
		out_user_rec->alt_data_loc = NULL;
	}

	if ( CFDictionaryContainsKey(in_dict, CFSTR(kXMLKeyAltDataStoreLoc)) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR( kXMLKeyAltDataStoreLoc ) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			char *c_str = (char*)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
			if ( c_str && strlen(c_str) )
				out_user_rec->alt_data_loc = i_strdup(c_str);

			if (out_user_dict != NULL)
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyAltDataStoreLoc), cf_str_ref);
		}
	}
} /* od_get_alt_loc */


/* ------------------------------------------------------------------
 *	od_get_mail_quota ()
 * ------------------------------------------------------------------*/

static void od_get_mail_quota ( CFDictionaryRef in_dict, CFMutableDictionaryRef out_user_dict, struct od_user *out_user_rec )
{
	/* Default value */
	out_user_rec->mail_quota = 0;

	if ( CFDictionaryContainsKey( in_dict, CFSTR( kXMLKeyDiskQuota ) ) ) {
		CFStringRef	cf_str_ref = (CFStringRef)CFDictionaryGetValue( in_dict, CFSTR( kXMLKeyDiskQuota ) );
		if ( (cf_str_ref != NULL) && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			char *c_str = (char *)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
			if ( c_str && strlen(c_str) )
				out_user_rec->mail_quota = atol(c_str);

			if (out_user_dict != NULL)
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyDiskQuota), cf_str_ref);
		}
	}
} /* od_get_mail_quota */


/* ------------------------------------------------------------------
 *	get_mail_attribute_values ()
 * ------------------------------------------------------------------*/

static CFMutableDictionaryRef get_mail_attribute_values ( struct auth_request *in_request ATTR_UNUSED, const char *in_mail_attribute, struct od_user *out_user_rec )
{
	CFMutableDictionaryRef	cf_dict_out	= NULL;

	i_assert(in_mail_attribute != NULL);

	unsigned long data_len = strlen( in_mail_attribute );
	CFDataRef cf_data_ref = CFDataCreate( NULL, (const UInt8 *)in_mail_attribute, data_len );
	if ( !cf_data_ref )
		return( NULL );

	CFPropertyListRef cf_plist_ref = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cf_data_ref, kCFPropertyListImmutable, NULL );
	if ( cf_plist_ref ) {
		if ( CFDictionaryGetTypeID() == CFGetTypeID(cf_plist_ref) ) {
			/* dictionary with values to be used to migrate locally */
			cf_dict_out = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			if ( cf_dict_out ) {
				/* set new version tag */
				CFDictionaryAddValue( cf_dict_out, CFSTR(kXMLKeyAttrVersion), CFSTR(kXMLValueVersion2) );

				od_get_acct_state( (CFDictionaryRef)cf_plist_ref, cf_dict_out );
				od_get_auto_forward( (CFDictionaryRef)cf_plist_ref, cf_dict_out );
				od_get_imap_login( (CFDictionaryRef)cf_plist_ref, cf_dict_out );
				od_get_pop3_login( (CFDictionaryRef)cf_plist_ref, cf_dict_out );
				od_get_migration( (CFDictionaryRef)cf_plist_ref, cf_dict_out, out_user_rec );
				od_get_acct_loc( (CFDictionaryRef)cf_plist_ref, cf_dict_out, out_user_rec );
				od_get_alt_loc( (CFDictionaryRef)cf_plist_ref, cf_dict_out, out_user_rec );
				od_get_mail_quota( (CFDictionaryRef)cf_plist_ref, cf_dict_out, out_user_rec );
			}
		}
		CFRelease( cf_plist_ref );
	}
	CFRelease( cf_data_ref );

	return( cf_dict_out );
} /* get_mail_attribute_values */


/* ------------------------------------------------------------------
 *	od_get_attr_from_record ()
 * ------------------------------------------------------------------*/

static CFStringRef od_get_attr_from_record ( struct auth_request *in_request, ODRecordRef in_rec_ref, CFStringRef in_attr )
{
	CFErrorRef cf_err_ref = NULL;

	CFArrayRef cf_arry_values = ODRecordCopyValues( in_rec_ref, in_attr, &cf_err_ref );
	if ( cf_arry_values == NULL ) {
		/* print the error and bail */
		if (in_request->set->debug)
			db_od_print_cf_error( in_request, cf_err_ref,
					t_strconcat("Unable to extract attribute ", CFStringGetCStringPtr(in_attr, kCFStringEncodingMacRoman), NULL) );
		return( NULL );
	}

	if ( CFArrayGetCount( cf_arry_values ) > 1 ) {
		auth_request_log_error(in_request, "od", "multiple attribute values (%d) found in record user record: %s for attribute: %s",
					(int)CFArrayGetCount( cf_arry_values ),
					CFStringGetCStringPtr( ODRecordGetRecordName( in_rec_ref ), kCFStringEncodingUTF8 ),
					CFStringGetCStringPtr( in_attr, kCFStringEncodingUTF8 ) );
		CFRelease( cf_arry_values );
		return( NULL );
	}

	CFStringRef cf_str_out = CFArrayGetValueAtIndex( cf_arry_values, 0 );
	CFRetain( cf_str_out );

	CFRelease( cf_arry_values );

	return( cf_str_out );
} /*  od_get_attr_from_record */


/* Begin DS SPI Glue */
#include <kvbuf.h>
#include <DSlibinfoMIG.h>
#include <DirectoryService/DirectoryService.h>

extern mach_port_t _ds_port;
extern int _ds_running();

static kern_return_t
get_procno( const char *procname, int32_t *procno )
{
	kern_return_t		status;
	security_token_t	token;
	bool				lookAgain;

	do {
		lookAgain = false;

    	if (_ds_running() == 0) return KERN_FAILURE;
    	if (_ds_port == MACH_PORT_NULL) return KERN_FAILURE;

		status = libinfoDSmig_GetProcedureNumber( _ds_port, (char *) procname, procno, &token );
		switch( status )
		{
			case MACH_SEND_INVALID_DEST:
			case MIG_SERVER_DIED:
				mach_port_mod_refs( mach_task_self(), _ds_port, MACH_PORT_RIGHT_SEND, -1 );
				_ds_port = MACH_PORT_NULL;
				lookAgain = true;
				break;

			case KERN_SUCCESS:
				// is there a security call to parse this private token?
				if ( token.val[0] != 0 ) {
					(*procno) = -1;
					status = KERN_FAILURE;
				}
				break;

			default:
				break;
		}
	} while ( lookAgain == true );

	return status;
} /* get_procno */

static kern_return_t
ds_lookup( int32_t procno, kvbuf_t *request, kvarray_t **answer )
{
	kern_return_t			status;
	security_token_t		token;
	bool					lookAgain;
	mach_msg_type_number_t	oolen		= 0;
	vm_address_t			oobuf		= 0;
	char					ilbuf[MAX_MIG_INLINE_DATA];
	mach_msg_type_number_t	illen		= 0;
	
	do {
		lookAgain = false;

    	if ( _ds_running() == 0 ) return KERN_FAILURE;
    	if ( _ds_port == MACH_PORT_NULL ) return KERN_FAILURE;
		if ( request == NULL ) return KERN_FAILURE;
		
		status = libinfoDSmig_Query( _ds_port, procno, request->databuf, request->datalen, ilbuf, &illen, &oobuf, &oolen, &token );
		switch( status )
		{
			case MACH_SEND_INVALID_DEST:
			case MIG_SERVER_DIED:
				mach_port_mod_refs( mach_task_self(), _ds_port, MACH_PORT_RIGHT_SEND, -1 );
				_ds_port = MACH_PORT_NULL;
				lookAgain = true;
				break;
				
			case KERN_SUCCESS:
				// is there a security call to parse this private token?
				if ( token.val[0] == 0 ) {
					if ( answer != NULL ) {
						kvbuf_t *tempBuf;
						
						if ( oolen != 0 ) {
							tempBuf = kvbuf_init( (char *)oobuf, (uint32_t) oolen );
						}
						else {
							tempBuf = kvbuf_init( ilbuf, illen );
						}
						
						(*answer) = kvbuf_decode( tempBuf );
						if ( (*answer) == NULL ) {
							kvbuf_free( tempBuf );
						}
					}
				}
				else {
					// response came from a process not running as root
					procno = -1;
					status = KERN_FAILURE;
				}
				break;
				
			default:
				break;
		}
	} while ( lookAgain == true );
	
	if ( oolen != 0 ) {
		vm_deallocate( mach_task_self(), oobuf, oolen );
	}
	
	return status;
} /* ds_lookup */


/* ------------------------------------------------------------------
 *	getpwnam_ext_real ()
 * ------------------------------------------------------------------*/

static kvarray_t *
getpwnam_ext_real( const char *name )
{
	static int32_t 			procno		= -1;
	static int32_t			initProc	= -1;
	static bool				setupList	= FALSE;
	kvarray_t				*response	= NULL;
	kern_return_t			status;
	
	if ( name == NULL ) {
		/* reset cached state */
		procno = -1;
		initProc = -1;
		setupList = FALSE;
		return NULL;
	}
	
	if ( procno == -1 ) {
		status = get_procno( "getpwnam_ext", &procno );
		if ( status != KERN_SUCCESS ) return NULL;
	}
	
	if ( initProc == -1 ) {
		status = get_procno( "getpwnam_initext", &initProc );
		if ( status != KERN_SUCCESS ) return NULL;
	}
			
	if (!setupList) {
		kvbuf_t *reqTypes = kvbuf_new();
		
		/* The following are already included by default:
		 * kDSNAttrRecordName			- pw_name
		 * kDS1AttrPassword			- pw_pass
		 * kDS1AttrUniqueID			- pw_uid
		 * kDS1AttrPrimaryGroupID		- pw_gid
		 * kDS1AttrNFSHomeDirectory	- pw_dir
		 * kDS1AttrUserShell			- pw_shell
		 * kDS1AttrDistinguishedName	- pw_gecos
		 * kDS1AttrGeneratedUID		- pw_uuid
		 *
		 * kDSNAttrKeywords			- not included, please file radar against DirectoryService
		 *	kDSNAttrMetaNodeLocation	- as-is
		 */
		  
		kvbuf_add_dict( reqTypes );
		kvbuf_add_key( reqTypes, "additionalAttrs" );
		kvbuf_add_val( reqTypes, kDS1AttrMailAttribute );
		kvbuf_add_val( reqTypes, kDSNAttrEMailAddress );
		kvbuf_add_val( reqTypes, kDS1AttrFirstName );
		kvbuf_add_val( reqTypes, kDS1AttrLastName );
		
		status = ds_lookup( initProc, reqTypes, NULL );
		kvbuf_free(reqTypes);
		if ( status != KERN_SUCCESS ) return NULL;
		setupList = TRUE;
	}
	
	kvbuf_t *request = kvbuf_query_key_val( "login", name );
	if ( request != NULL ) {
		ds_lookup( procno, request, &response );
		kvbuf_free( request );
	}
	
	return response;
} /* getpwnam_ext_real */

static kvarray_t *
getpwnam_ext ( const char *name )
{
	kvarray_t *response = NULL;

	if (name != NULL) {
		response = getpwnam_ext_real(name);
		if (response == NULL) {
			/* reset cached state */
			(void) getpwnam_ext_real(NULL);

			/* retry once */
			response = getpwnam_ext_real(name);
		}
	}

	return response;
} /* getpwnam_ext */
/* End DS SPI Glue */


/* ------------------------------------------------------------------
 *	ds_get_value ()
 * ------------------------------------------------------------------*/

static const char *ds_get_value ( struct auth_request *in_request, const kvdict_t *in_dict, const char *in_attr, bool first_of_many )
{
	const char *value = NULL;
	uint32_t i;

	for (i = 0; i < in_dict->kcount; i++) {
		if (!strcmp(in_dict->key[i], in_attr)) {
			if (in_dict->vcount[i] == 1)
				value = in_dict->val[i][0];
			else if (in_dict->vcount[i] == 0)
				auth_request_log_debug(in_request, "od[getpwnam_ext]", "no value found for attribute %s", in_attr);
			else if (first_of_many) {
				if ( strcmp(in_attr, "pw_name") )
					value = in_dict->val[i][0];
				else {
					uint32_t j;
					value = in_dict->val[i][0];
					for (j = 0; j < in_dict->vcount[i]; j++) {
						if ( strchr(in_dict->val[i][j], '@') == 0 ) {
							value = in_dict->val[i][j];
							break;
						}
					}
				}
			}
			else
				auth_request_log_debug(in_request, "od[getpwnam_ext]", "multiple values (%u) found for attribute %s", in_dict->vcount[i], in_attr);
			break;
		}
	}
	if (i >= in_dict->kcount)
		auth_request_log_debug(in_request, "od[getpwnam_ext]", "no attribute %s in user record", in_attr);

	return value;
} /* ds_get_value */


/* ------------------------------------------------------------------
 *	ds_get_user ()
 * ------------------------------------------------------------------*/

static struct od_user *ds_get_user ( struct auth_request *in_request, struct db_od *in_od_info, const char *in_user_name )
{
	struct od_user *user_record = NULL;
	kvarray_t *user_data;

	DTRACE_OD_LOOKUP_START(in_od_info, (char *) in_user_name);

	errno = 0;
	user_data = getpwnam_ext(in_user_name);
	if (user_data != NULL) {
		if (user_data->count == 1) {
			kvdict_t *user_dict = &user_data->dict[0];
			const char *value;

			user_record = i_new(struct od_user, 1);
			user_record->refcount = 1;
			user_record->create_time = time(NULL);

			// kDS1AttrUniqueID
			value = ds_get_value(in_request, user_dict, "pw_uid", FALSE);
			if (value)
				user_record->user_uid = atoi(value);

			// kDS1AttrPrimaryGroupID
			value = ds_get_value(in_request, user_dict, "pw_gid", FALSE);
			if (value)
				user_record->user_gid = atoi(value);

			// kDS1AttrGeneratedUID
			value = ds_get_value(in_request, user_dict, "pw_uuid", FALSE);
			if (value)
				user_record->user_guid = i_strdup(value);

			// kDSNAttrRecordName
			value = ds_get_value(in_request, user_dict, "pw_name", TRUE);
			if (value)
				user_record->record_name = i_strdup(value);

			// kDS1AttrMailAttribute
			user_record->acct_state |= account_enabled;
			user_record->acct_state |= imap_enabled;
			user_record->acct_state |= pop_enabled;

			if ( !od_get_attributes_local(in_request, user_record, user_record->user_guid) ) {
				auth_request_log_debug(in_request, "od[getpwnam_ext]", "no local settings found for guid: %s (%s)", user_record->user_guid, user_record->record_name);
				value = ds_get_value(in_request, user_dict, kDS1AttrMailAttribute, FALSE);
				if (value) {
					CFMutableDictionaryRef user_dict = get_mail_attribute_values(in_request, value, user_record);
					od_set_attributes_local(in_request, user_dict, user_record);
					if (user_dict)
						CFRelease(user_dict);
				}
			}else
				auth_request_log_debug(in_request, "od[getpwnam_ext]", "found local settings found for guid: %s (%s)", user_record->user_guid, user_record->record_name);

			auth_request_log_debug(in_request, "od[getpwnam_ext]",
				"uid=%d gid=%d state=%#x quota=%d guid=%s name=%s loc=%s alt=%s",
				user_record->user_uid, user_record->user_gid,
				user_record->acct_state, user_record->mail_quota,
				user_record->user_guid, user_record->record_name,
				user_record->acct_loc, user_record->alt_data_loc);
		} else if (user_data->count == 0)
			auth_request_log_error(in_request, "od[getpwnam_ext]", "no user record found");
		else
			auth_request_log_error(in_request, "od[getpwnam_ext]", "multiple user records (%u) found", user_data->count);

		kvarray_free(user_data);
	} else if (errno)
		auth_request_log_error(in_request, "od[getpwnam_ext]", "Unable to look up user record: %m");
	else
		auth_request_log_error(in_request, "od[getpwnam_ext]", "No record for user");

	DTRACE_OD_LOOKUP_FINISH(in_od_info, (char *) in_user_name, user_record);

	return user_record;
} /* ds_get_user */


/* ------------------------------------------------------------------
 *	od_read_user_settings ()
 * ------------------------------------------------------------------*/

static CFPropertyListRef od_read_user_settings ( struct auth_request *in_request, const char *file, CFPropertyListMutabilityOptions in_opts )
{
	SInt32 err = noErr;

	CFURLRef cf_url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)file, strlen(file), FALSE);
	if ( !cf_url ) {
		auth_request_log_error(in_request, "od-local", "Could not create URL from %s", file);
		return( NULL );
	}

	CFDataRef cf_data_ref = NULL;
	if ( !CFURLCreateDataAndPropertiesFromResource(NULL, cf_url, &cf_data_ref, NULL, NULL, &err) ) {
		auth_request_log_error(in_request, "od-local", "Unable to read data from %s (error %d)", file, (int) err);
		CFRelease(cf_url);
		return( NULL );
	}

	CFPropertyListRef cf_prop_list = NULL;
	if ( cf_data_ref ) {
		cf_prop_list = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cf_data_ref, in_opts, NULL);
		if ( cf_prop_list == NULL )
			auth_request_log_debug(in_request, "od-local", "empty user settings file %s", file);

		CFRelease(cf_data_ref);
	} else
		auth_request_log_debug(in_request, "od-local", "unable to create CFData ref for %s", file);

	CFRelease(cf_url);
	return( cf_prop_list );
} /* od_read_user_settings */


/* ------------------------------------------------------------------
 *	od_write_user_settings ()
 * ------------------------------------------------------------------*/

static void od_write_user_settings ( struct auth_request *in_request, const char *in_file, CFPropertyListRef in_data )
{
	SInt32 err = noErr;

	CFURLRef cf_url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)in_file, strlen(in_file), FALSE);
	if ( !cf_url ) {
		auth_request_log_error(in_request, "od-local", "could not create URL from %s", in_file);
		return;
	}

	CFDataRef cf_data_ref = CFPropertyListCreateXMLData(NULL, in_data);
	if ( cf_data_ref ) {
		if ( !CFURLWriteDataAndPropertiesToResource(cf_url, cf_data_ref, NULL, &err) )
			auth_request_log_error(in_request, "od-local", "could not write to %s (error: %d)", in_file, (int)err);

		CFRelease(cf_data_ref);
	}

	int fd = open(in_file, O_RDONLY);
	if (fd != -1) {
		fchown(fd, 27, 6);
		fchmod(fd, 0660);
		close(fd);
	}

	CFRelease(cf_url);
} /* od_write_user_settings */


/* ------------------------------------------------------------------
 *	od_get_attributes_local ()
 * ------------------------------------------------------------------*/

static bool od_get_attributes_local ( struct auth_request *in_request, struct od_user *out_user_rec, const char *in_user_guid )
{
	bool out_result = FALSE;

	/* look in local file first */
	CFDictionaryRef cf_dict_data = (CFDictionaryRef)od_read_user_settings( in_request, users_path, kCFPropertyListImmutable );
	if ( !cf_dict_data )
		return( FALSE );

	CFStringRef cf_str_ref = CFStringCreateWithCString( NULL, in_user_guid, kCFStringEncodingUTF8 );
	if ( cf_str_ref != NULL ) {
		if ( CFDictionaryContainsKey( cf_dict_data, cf_str_ref ) ) {
			CFDictionaryRef cf_dict_user = (CFDictionaryRef)CFDictionaryGetValue( cf_dict_data, cf_str_ref );
			if ( cf_dict_user ) {
				if ( CFGetTypeID( cf_dict_user ) == CFDictionaryGetTypeID() ) {
					auth_request_log_debug(in_request, "od", "found local settings for guid: %s (%s)", in_user_guid, out_user_rec->record_name);
					od_get_alt_loc( cf_dict_user, NULL, out_user_rec );
					od_get_mail_quota( cf_dict_user, NULL, out_user_rec );
					od_get_migration( cf_dict_user, NULL, out_user_rec );
					out_result = TRUE;
				}
			}
		}
		CFRelease(cf_str_ref);
	}
	CFRelease(cf_dict_data);

	return( out_result );
} /* od_get_attributes_local */


/* ------------------------------------------------------------------
 *	od_set_attributes_local ()
 * ------------------------------------------------------------------*/

static void od_set_attributes_local ( struct auth_request *in_request, CFMutableDictionaryRef in_user_dict, struct od_user *in_user_rec )
{
	CFMutableDictionaryRef cf_mut_dict = NULL;

	i_assert(in_user_dict != NULL);
	i_assert(in_user_rec != NULL);

	/* get current settings from local file */
	CFDictionaryRef cf_dict_ref = (CFDictionaryRef)od_read_user_settings(in_request, users_path, kCFPropertyListMutableContainers);
	if ( cf_dict_ref ) {
		cf_mut_dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, cf_dict_ref);
		CFRelease( cf_dict_ref );
	} else
		cf_mut_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if ( !cf_mut_dict ) {
		auth_request_log_debug(in_request, "od-local", "unable to create local user mutable dictionary ref");
		return;
	}

	CFStringRef cf_str_ref = CFStringCreateWithCString( NULL, in_user_rec->user_guid, kCFStringEncodingUTF8 );
	if ( cf_str_ref ) {
		CFDictionaryAddValue( cf_mut_dict, cf_str_ref, in_user_dict );
		od_write_user_settings(in_request, users_path, (CFPropertyListRef)cf_mut_dict );
		CFRelease(cf_str_ref);
	}

	CFRelease(cf_mut_dict);
} /* od_set_attributes_local */


/* ------------------------------------------------------------------
 *	od_get_user ()
 * ------------------------------------------------------------------*/

static struct od_user *od_get_user ( struct auth_request *in_request, struct db_od *in_od_info, const char *in_user_name )
{
	size_t			str_size		= 0;
	ODRecordRef		od_rec_ref		= NULL;
	CFStringRef		cf_str_ref		= NULL;
	CFStringRef		cf_str_value	= NULL;
	CFErrorRef		cf_err_ref		= NULL;
	struct od_user *out_user_rec	= NULL;

	cf_str_ref = CFStringCreateWithCString( NULL, in_user_name, kCFStringEncodingUTF8 );
	if ( !cf_str_ref ) {
		auth_request_log_error(in_request, "od", "unable to create user name CFStringRef");
		return( NULL );
	}

	/* create user record */
	out_user_rec = i_new( struct od_user, 1 );
	i_assert(out_user_rec != NULL);
	out_user_rec->refcount = 1;
	out_user_rec->create_time = time(NULL);

	DTRACE_OD_LOOKUP_START(in_od_info, (char *) in_user_name);

	CFTypeRef cf_type_ref[]	= { CFSTR(kDSAttributesStandardAll) };
	CFArrayRef cf_arry_ref = CFArrayCreate( NULL, cf_type_ref, 1, &kCFTypeArrayCallBacks );
	ODQueryRef cf_query_ref = ODQueryCreateWithNode( NULL, in_od_info->od_node_ref, CFSTR(kDSStdRecordTypeUsers), CFSTR(kDSNAttrRecordName),
											kODMatchInsensitiveEqualTo, cf_str_ref, cf_arry_ref, 100, &cf_err_ref );
	CFRelease( cf_arry_ref );
	CFRelease( cf_str_ref );
	if ( cf_query_ref ) {
		CFArrayRef cf_arry_result = ODQueryCopyResults( cf_query_ref, false, &cf_err_ref );
		if ( cf_arry_result ) {
			if ( CFArrayGetCount( cf_arry_result ) == 1 ) {
				od_rec_ref = (ODRecordRef)CFArrayGetValueAtIndex( cf_arry_result, 0 );
				CFRetain(od_rec_ref);
			} else {
				if ( CFArrayGetCount( cf_arry_result ) == 0 )
					auth_request_log_error(in_request, "od", "no user record found for: %s", in_user_name);
				else
					auth_request_log_error(in_request, "od", "multiple user records (%ld) found for: %s", CFArrayGetCount( cf_arry_result ), in_user_name);
			}
			CFRelease(cf_arry_result);
		}
		else
			db_od_print_cf_error( in_request, cf_err_ref, "OD Query Copy Results failed" );

		CFRelease( cf_query_ref );
	} else
		db_od_print_cf_error( in_request, cf_err_ref, "OD Query Create With Node failed" );

	if ( !od_rec_ref ) {
		db_od_print_cf_error( in_request, cf_err_ref, "Unable to lookup user record" );
		DTRACE_OD_LOOKUP_FINISH(in_od_info, (char *) in_user_name, NULL);
		i_free(out_user_rec);
		return( NULL );
	}

	cf_str_value = od_get_attr_from_record( in_request, od_rec_ref, CFSTR(kDS1AttrUniqueID) );
	if ( cf_str_value ) {
		out_user_rec->user_uid = CFStringGetIntValue( cf_str_value );
		CFRelease( cf_str_value );
	}

	cf_str_value = od_get_attr_from_record( in_request, od_rec_ref, CFSTR(kDS1AttrPrimaryGroupID) );
	if ( cf_str_value ) {
		out_user_rec->user_gid = CFStringGetIntValue( cf_str_value );
		CFRelease( cf_str_value );
	}

	/* get guid */
	cf_str_value = od_get_attr_from_record( in_request, od_rec_ref, CFSTR(kDS1AttrGeneratedUID) );
	if ( cf_str_value ) {
		str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength( cf_str_value ), kCFStringEncodingUTF8) + 1;
		out_user_rec->user_guid = i_malloc( str_size );
		if ( out_user_rec->user_guid != NULL )
			CFStringGetCString( cf_str_value, out_user_rec->user_guid, str_size, kCFStringEncodingUTF8 );

		CFRelease( cf_str_value );
	}

	/* get record name */
	cf_str_value = ODRecordGetRecordName( od_rec_ref );
	if ( cf_str_value != NULL ) {
		str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength( cf_str_value ), kCFStringEncodingUTF8) + 1;
		out_user_rec->record_name = i_malloc( str_size );
		if ( out_user_rec->record_name != NULL )
			CFStringGetCString( cf_str_value, out_user_rec->record_name, str_size, kCFStringEncodingUTF8 );
	}

	/* get mail attribute */
	out_user_rec->acct_state |= account_enabled;
	out_user_rec->acct_state |= imap_enabled;
	out_user_rec->acct_state |= pop_enabled;
	if ( !od_get_attributes_local(in_request, out_user_rec, out_user_rec->user_guid) ) {
		auth_request_log_debug(in_request, "od", "no local settings found for guid: %s (%s)", out_user_rec->user_guid, out_user_rec->record_name);
		cf_str_value = od_get_attr_from_record( in_request, od_rec_ref, CFSTR(kDS1AttrMailAttribute) );
		if ( cf_str_value ) {
			char *c_str = (char*)CFStringGetCStringPtr( cf_str_value, kCFStringEncodingMacRoman );
			CFMutableDictionaryRef user_dict = get_mail_attribute_values(in_request, c_str, out_user_rec);
			od_set_attributes_local(in_request, user_dict, out_user_rec);
			if (user_dict)
				CFRelease(user_dict);
			CFRelease( cf_str_value );
		}
	}
	CFRelease(od_rec_ref);

	DTRACE_OD_LOOKUP_FINISH(in_od_info, (char *) in_user_name, out_user_rec);

	return( out_user_rec );
} /* od_get_user */


/* ------------------------------------------------------------------
 *	db_od_sacl_check ()
 * ------------------------------------------------------------------*/

void db_od_sacl_check ( struct auth_request *in_request, struct od_user *in_od_user, const char *in_group )
{
	int		err		= 0;
	int		result	= 0;
	uuid_t	guid;

	/* quick migration check */
	struct stat st;
	if (stat("/var/db/.mailmigration.plist", &st) == 0) {
		if ( !(in_od_user->acct_state & acct_migrated) ) {
			in_od_user->acct_state &= ~account_enabled;
			in_od_user->acct_state &= ~imap_enabled;
			in_od_user->acct_state &= ~pop_enabled;
			return;
		}
	}

	if ( mail_sacl_enabled == FALSE ) {
		if ( sacl_check_delta > time(NULL) ) {
			in_od_user->acct_state |= account_enabled;
			in_od_user->acct_state |= imap_enabled;
			in_od_user->acct_state |= pop_enabled;
			return;
		}
		sacl_check_delta = time(NULL) + 30;
	}

	DTRACE_OD_SACL_START(in_od_user, (char *) in_group);

	/* we should already have this from previous user lookup */
	if ( in_od_user->user_guid != NULL )
		err = mbr_string_to_uuid( (const char *)in_od_user->user_guid, guid );
	else if ( in_od_user->record_name != NULL ) /* get the uuid for user */
		err = mbr_user_name_to_uuid( in_od_user->record_name, guid );
	else {
		in_od_user->acct_state |= sacl_not_member;
		in_od_user->acct_state &= ~account_enabled;
		in_od_user->acct_state &= ~imap_enabled;
		in_od_user->acct_state &= ~pop_enabled;

		auth_request_log_error(in_request, "od", "no user record name or user uuid for SACL checks");
		DTRACE_OD_SACL_FINISH(in_od_user, (char *) in_group, -1);
		return;
	}

	/* bail fi we couldn't turn user into uuid settings form user record */
	if ( err != 0 ) {
		in_od_user->acct_state |= sacl_not_member;
		in_od_user->acct_state &= ~account_enabled;
		in_od_user->acct_state &= ~imap_enabled;
		in_od_user->acct_state &= ~pop_enabled;

		auth_request_log_error(in_request, "od", "mbr_user_name_to_uuid failed for user: %s (%s)", in_od_user->record_name, strerror(err));
		DTRACE_OD_SACL_FINISH(in_od_user, (char *) in_group, -2);
		return;
	}

	/* check the mail SACL */
	err = mbr_check_service_membership( guid, in_group, &result );

	/* service ACL is enabled */
	if ( err == 0 ) {
		mail_sacl_enabled = TRUE;

		auth_request_log_debug(in_request, "od", "mail SACL is enabled; overriding settings in user record");

		/* set SACL enabled flag */
		in_od_user->acct_state |= sacl_enabled;

		/* check membership */
		if ( result != 0 ) {
			/* we are a member, enable all mail services */
			in_od_user->acct_state |= account_enabled;
			in_od_user->acct_state |= imap_enabled;
			in_od_user->acct_state |= pop_enabled;

			DTRACE_OD_SACL_FINISH(in_od_user, (char *) in_group, 1);
		} else {
			/* we are not a member override any settings form user record */
			in_od_user->acct_state |= sacl_not_member;
			in_od_user->acct_state &= ~account_enabled;
			in_od_user->acct_state &= ~imap_enabled;
			in_od_user->acct_state &= ~pop_enabled;

			DTRACE_OD_SACL_FINISH(in_od_user, (char *) in_group, 0);
		}
	} else {
		/* set SACL -not- enabled flag */
		in_od_user->acct_state |= account_enabled;
		in_od_user->acct_state |= imap_enabled;
		in_od_user->acct_state |= pop_enabled;

		mail_sacl_enabled = FALSE;

		auth_request_log_debug(in_request, "od", "mail SACL is not enabled; error=%d", err);

		DTRACE_OD_SACL_FINISH(in_od_user, (char *) in_group, -3);
	}
} /* db_od_sacl_check */


/* ------------------------------------------------------------------
 *	db_od_user_lookup ()
 *
 * Caller must release the returned object via db_od_user_unref().
 * ------------------------------------------------------------------*/

struct od_user *db_od_user_lookup ( struct auth_request *in_request, struct db_od *in_od_info, const char *in_user_name, bool in_is_auth )
{
	char *key = NULL;
	struct od_user	   *out_user		= NULL;

	/* TODO: use auth_cache */

	/* does user exist in hash table */
	auth_request_log_debug(in_request, "od", "cache lookup for user %s", in_user_name);
	if (hash_table_lookup_full(in_od_info->users_table, in_user_name, (void **) &key, (void **) &out_user)) {
		/* is the cached entry fresh or stale? */
		time_t now = time(NULL);
		time_t expiry = out_user->create_time;
		if ( out_user->acct_state & account_enabled )
			expiry += in_od_info->pos_cache_ttl;
		else
			expiry += in_od_info->neg_cache_ttl;
		if (expiry > now) {

			++out_user->refcount;

			DTRACE_OD_LOOKUP_CACHED(in_od_info, (char *) in_user_name, out_user);

			/* check for SACL changes when authenticating */
			if (in_is_auth || (in_request->mech_name && (strcmp(in_request->mech_name, "GSSAPI") == 0)))
				db_od_sacl_check( in_request, out_user, "mail" );

			auth_request_log_debug(in_request, "od", "found user %s in cache as %s", out_user->record_name, in_user_name );
			return( out_user );
		} else {
			auth_request_log_debug(in_request, "od", "discarding cache entry for user %s as %s (age=%ld)", out_user->record_name, in_user_name, now - out_user->create_time );
			hash_table_remove(in_od_info->users_table, in_user_name);
			i_free(key);
			db_od_user_unref(&out_user);
		}
	} else
		auth_request_log_debug(in_request, "od", "user %s not cached (%d)", in_user_name, hash_table_count(in_od_info->users_table) );

	/* lookup user in OD */
	auth_request_log_debug(in_request, "od", "directory lookup for user %s", in_user_name );
	out_user = in_od_info->use_getpwnam_ext ?
		ds_get_user(in_request, in_od_info, in_user_name) :
		od_get_user(in_request, in_od_info, in_user_name);

	if ( out_user ) {
		if (in_od_info->pos_cache_ttl > 0 && in_od_info->neg_cache_ttl > 0)
			od_add_user( in_request, in_od_info, out_user, in_user_name );

		/* do SACL check when authenticating */
		if (in_is_auth || (in_request->mech_name && (strcmp(in_request->mech_name, "GSSAPI") == 0)))
			db_od_sacl_check( in_request, out_user, "mail" );
	}

	return( out_user );
} /* db_od_user_lookup */


/* ------------------------------------------------------------------
 *	is_acct_enabled ()
 * ------------------------------------------------------------------*/

bool is_acct_enabled ( struct auth_request *in_request, od_acct_state in_acct_state, const char *in_user )
{
	const char *service	= in_request->service;

	/* make sure their account is enabled either in OD or via Server Admin SACL */
	if ( !(in_acct_state & account_enabled) ) {
		auth_request_log_info(in_request, "od", "mail account for: %s is not enabled", in_user );
		return( FALSE );
	}

	/* is the service enabled for this user */
	/* Server Admin SACLs override settings in OD */
	if ( (strcasecmp(service, "imap") == 0) && !(in_acct_state & imap_enabled) ) {
		auth_request_log_info(in_request, "od", "IMAP mail service for user account: %s is not enabled", in_user );
		return( FALSE );
	}

	if ( (strcasecmp(service, "pop3") == 0) && !(in_acct_state & pop_enabled) ) {
		auth_request_log_info(in_request, "od", "POP mail service for user account: %s is not enabled", in_user );
		return( FALSE );
	}

	return( TRUE );
} /* is_acct_enabled */


/* ------------------------------------------------------------------
 *	push_notify_init ()
 *
 * ------------------------------------------------------------------*/

void push_notify_init ( struct od_user *in_user_info )
{
	int notify_enabled = 0;
	struct sockaddr_un sock_addr;
	socklen_t sock_len=0;
    msg_data_t message_data;
	int soc;
	int rc;
	const char *socket_path = "/var/dovecot/push_notify";

	/* read enabled flage from config plist */
	CFURLRef cf_url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)notify_path, strlen(notify_path), FALSE);
	if ( cf_url ) {
		SInt32 err = noErr;
		CFDataRef cf_data_ref = NULL;
		if ( CFURLCreateDataAndPropertiesFromResource(NULL, cf_url, &cf_data_ref, NULL, NULL, &err) ) {
			if ( cf_data_ref ) {
				CFDictionaryRef cf_dict = (CFDictionaryRef)CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cf_data_ref, kCFPropertyListImmutable, NULL);
				if ( cf_dict ) {
					if ( CFDictionaryContainsKey(cf_dict, CFSTR(kPushNotifyEnabled)) ) {
						CFBooleanRef cf_bool = (CFBooleanRef)CFDictionaryGetValue( cf_dict, CFSTR(kPushNotifyEnabled) );
						if ( (cf_bool != NULL) && (CFGetTypeID( cf_bool ) == CFBooleanGetTypeID()) )
							notify_enabled = (kCFBooleanTrue == cf_bool);
					}
					CFRelease(cf_dict);
				}
				CFRelease(cf_data_ref);
			}
		}
		CFRelease(cf_url);
	}

	if ( !notify_enabled )
		return;		/* push notification not enabled */

	if ( in_user_info->user_guid == NULL ) {
		i_warning( "no guid found for user: %s", in_user_info->record_name );
		return;
	}

	soc = socket( AF_UNIX, SOCK_DGRAM, 0 );
	if ( soc < 0 ) {
		/* warn that connect failed but do not fail the plugin or message will not get delivered */
		i_warning( "open notify socket failed(%d): %m", soc );
		return;
	}

	memset( &sock_addr, 0, sizeof(struct sockaddr_un));
	sock_addr.sun_family = AF_UNIX;
	strlcpy( sock_addr.sun_path, socket_path, sizeof(sock_addr.sun_path) );
	sock_len = sizeof(sock_addr.sun_family) + strlen(sock_addr.sun_path) + 1;
	rc = connect(soc, (struct sockaddr *) &sock_addr,  sock_len);

	if ( rc < 0 ) {
		/* warn that connect failed but do not fail the plugin or message will not get delivered */
		i_warning("push: connect to notify socket %s failed: %m",
			  socket_path);
		close(soc);
		return;
	}

	/* set message data */
	memset(&message_data, 0, sizeof(struct msg_data_s));
	message_data.msg = 1; /* create node */
	strlcpy( message_data.d1, in_user_info->record_name, sizeof(message_data.d1) );

	rc = send(soc, (void *)&message_data, sizeof(message_data), 0);
	if ( rc < 0 ) {
		i_warning( "send to notify socket %s failed: %m", socket_path);
	}

	close(soc);
}

#endif
