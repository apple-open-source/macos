/*
 * Copyright (c) 2004-2011 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "sys_defs.h"
#include "aod.h"
#include "msg.h"
#include "mail_params.h"
#include "sacl_cache_clnt.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dtrace-postfix.h>

#include <membership.h>
#include <membershipPriv.h>

#include <CoreDaemon/CoreDaemon.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <DirectoryService/DirServicesConst.h>

/* -----------------------------------------------------------------
	Prototypes 
   ----------------------------------------------------------------- */

static void	print_cf_error			( CFErrorRef in_cf_err_ref, const char *in_user_name, const char *in_default_str );
static int	get_user_attributes		( ODNodeRef in_node_ref, const char *in_user_name, struct od_user_opts *in_out_opts );
void		get_acct_state			( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict, struct od_user_opts *in_out_opts );
static void	get_auto_forward_addr	( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict, struct od_user_opts *in_out_opts );
static void	get_alt_loc				( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict );
static void	get_mail_quota			( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict );
static bool get_attributes_local	( struct od_user_opts *in_out_opts, const char *in_user_guid );
static void set_attributes_local	( CFMutableDictionaryRef in_user_dict, const char *in_user_guid );

static CFStringRef get_attr_from_record ( ODRecordRef in_rec_ref, CFStringRef in_attr );
static CFMutableDictionaryRef get_mail_attribute_values	( const char *in_mail_attribute, struct od_user_opts *in_out_opts );

const char *user_settings_path = "/var/db/.mailusersettings.plist";

/* Begin DS SPI Glue */
#include <kvbuf.h>
#include <DSlibinfoMIG.h>
#include <DirectoryService/DirectoryService.h>

extern mach_port_t _ds_port;
extern int _ds_running();

int g_bad_recip_cntr	= 0;
char g_client_addr[16] = "";
XSEventPortRef	gEventPort = NULL;

/* send server events
 *	event code 1: bad recipient, possible directory harvesting attack
 *	event code 2: failed authentication, possible SMTP relay password attach
 */
 
void send_server_event ( const eEventCode in_event_code, const char *in_name, const char *in_addr )
{
	CFTypeRef keys[2];
	CFTypeRef values[2];
	CFStringRef cfstr_addr = NULL;
	CFStringRef cfstr_event = NULL;

	if ( !strlen(g_client_addr) || (strcmp(g_client_addr, in_addr) != 0) ) {
		strlcpy(g_client_addr, in_addr, sizeof g_client_addr);
		g_bad_recip_cntr = 0;
	}

	if ( g_bad_recip_cntr++ < 4 )
		return;
	else
		sleep( g_bad_recip_cntr >= 10 ? 10 : g_bad_recip_cntr );

	/* create a port to the event server */
	if ( gEventPort == NULL )
		gEventPort = XSEventPortCreate(nil);

	keys[0] = CFSTR("eventType");
	keys[1] = CFSTR("host_address");

	/* set event code string */
	switch ( in_event_code ) {
		case eBadRecipient:
			cfstr_event = CFStringCreateWithCString(NULL, "smtp.receive.badrecipient", kCFStringEncodingMacRoman);
			break;
		case eAuthFailure:
			cfstr_event = CFStringCreateWithCString(NULL, "auth.failure", kCFStringEncodingMacRoman);
			break;
		case eAuthSuccess:
			cfstr_event = CFStringCreateWithCString(NULL, "auth.success", kCFStringEncodingMacRoman);
			break;
		default:
			msg_warn("Warning: unknown sever event: %d", in_event_code);
			return;
	}

	cfstr_addr = CFStringCreateWithCString(NULL, in_addr, kCFStringEncodingMacRoman);

	values[0] = cfstr_event;
	values[1] = cfstr_addr;

     CFDictionaryRef dict_event = CFDictionaryCreate(NULL, keys, values, 
                                               sizeof(keys) / sizeof(keys[0]), 
                                               &kCFTypeDictionaryKeyCallBacks, 
                                               &kCFTypeDictionaryValueCallBacks); 
	
	/* send the event */
	(void)XSEventPortPostEvent(gEventPort, cfstr_event, dict_event);

	CFRelease(cfstr_addr);
	CFRelease(cfstr_event);
	CFRelease(dict_event);
} /* send_server_event */


void close_server_event_port ( void )
{
	if ( gEventPort != NULL )
		XSEventPortDelete(gEventPort);
} /* close_server_event_port */


__private_extern__ kern_return_t
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
}

__private_extern__ kern_return_t
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
}

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
		 * kDSNAttrRecordName		- pw_name
		 * kDS1AttrPassword			- pw_pass
		 * kDS1AttrUniqueID			- pw_uid
		 * kDS1AttrPrimaryGroupID	- pw_gid
		 * kDS1AttrNFSHomeDirectory	- pw_dir
		 * kDS1AttrUserShell		- pw_shell
		 * kDS1AttrDistinguishedName- pw_gecos
		 * kDS1AttrGeneratedUID		- pw_uuid
		 *
		 * kDSNAttrKeywords			- not included, please file radar against DirectoryService
		 * kDSNAttrMetaNodeLocation	- as-is
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
}

kvarray_t *
getpwnam_ext(const char *name)
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
}
/* End DS SPI Glue */

/* ------------------------------------------------------------------
 *	ds_get_value ()
 */
static const char *ds_get_value(const char *inUserID, const kvdict_t *in_dict, const char *in_attr, bool first_of_many)
{
	const char *value = NULL;
	int32_t i;

	for (i = 0; i < in_dict->kcount; i++) {
		if (!strcmp(in_dict->key[i], in_attr)) {
			if (in_dict->vcount[i] == 1)
				value = in_dict->val[i][0];
			else if (in_dict->vcount[i] == 0) {
				if ( strcmp( in_attr, kDS1AttrMailAttribute) != 0 )
					msg_info("od[getpwnam_ext]: no value found for attribute %s in record for user %s", in_attr, inUserID);
			} else if (first_of_many) {
				if ( strcmp(in_attr, "pw_name") )
					value = in_dict->val[i][0];
				else {
					int32_t j;
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
				msg_info("od[getpwnam_ext]: multiple values (%u) found for attribute %s in record for user %s", in_dict->vcount[i], in_attr, inUserID);
			break;
		}
	}
	if (i >= in_dict->kcount && strcmp(in_attr, kDS1AttrMailAttribute) != 0)
		msg_info("od[getpwnam_ext]: no attribute %s in record for user %s", in_attr, inUserID);

	return value;
} /* ds_get_value */

/* ------------------------------------------------------------------
 *	ads_get_uid ()
 */
uid_t ads_get_uid ( const char *inUserID )
{
	kvarray_t *user_data;
	uid_t uid = 0;

	assert(inUserID != NULL);

	errno = 0;
	user_data = getpwnam_ext(inUserID);
	if (user_data != NULL) {
		if (user_data->count == 1) {
			kvdict_t *user_dict = &user_data->dict[0];
			const char *value = ds_get_value(inUserID, user_dict, "pw_uid", TRUE);
			if (value)
				uid = atoi(value);
		} else if (user_data->count > 1)
			msg_error("od[getpwnam_ext]: multiple records (%u) found for user %s", user_data->count, inUserID);
		else if (!user_data->count)
			msg_error("od[getpwnam_ext]: no record found for user %s", inUserID);

		kvarray_free(user_data);
	} else if (errno)
		msg_error("od[getpwnam_ext]: Unable to look up user record %s: %m", inUserID);

	return(uid);
} /* ads_get_uid */

/* ------------------------------------------------------------------
 *	ads_getpwnam ()
 */
const char *ads_getpwnam ( const char *inUserID )
{
	kvarray_t *user_data;
	static char rec_name[512];

	assert(inUserID != NULL);
	memset(rec_name, 0, sizeof rec_name);

	errno = 0;
	user_data = getpwnam_ext(inUserID);
	if (user_data != NULL) {
		if (user_data->count == 1) {
			kvdict_t *user_dict = &user_data->dict[0];
			const char *value = ds_get_value(inUserID, user_dict, "pw_name", TRUE);
			if (value)
				strlcpy(rec_name, value, sizeof rec_name);
		} else if (user_data->count > 1)
			msg_error("od[getpwnam_ext]: multiple records (%u) found for user %s", user_data->count, inUserID);
		else if (!user_data->count)
			msg_error("od[getpwnam_ext]: no record found for user %s", inUserID);
		
		kvarray_free(user_data);
	} else if (errno)
		msg_error("od[getpwnam_ext]: Unable to look up user record %s: %m", inUserID);

	if (strlen(rec_name))
		return(rec_name);
	return( NULL );
} /* ads_getpwnam */

/* ------------------------------------------------------------------
 *	ads_get_user_options ()
 */
int ads_get_user_options(const char *inUserID, struct od_user_opts *in_out_opts)
{
	int out_status = 0;
	kvarray_t *user_data;
	char user_guid[ 64 ];

	assert(inUserID != NULL && in_out_opts != NULL);
	memset(in_out_opts, 0, sizeof *in_out_opts);
	in_out_opts->fAcctState = eUnknownAcctState;

	if (POSTFIX_OD_LOOKUP_START_ENABLED())
		POSTFIX_OD_LOOKUP_START((char *) inUserID, in_out_opts);

	errno = 0;
	user_data = getpwnam_ext(inUserID);
	if (user_data) {
		if (user_data->count == 1) {
			kvdict_t *user_dict = &user_data->dict[0];
			const char *value;

			/* get guid */
			memset(user_guid, 0, sizeof user_guid);
			value = ds_get_value(inUserID, user_dict, "pw_uuid", FALSE);
			if (value)
				strlcpy(user_guid, value, sizeof user_guid);

			if ( !get_attributes_local(in_out_opts, user_guid) ) {
				value = ds_get_value(inUserID, user_dict, kDS1AttrMailAttribute, FALSE);
				if (value) {
					CFMutableDictionaryRef user_dict = get_mail_attribute_values(value, in_out_opts);
					set_attributes_local(user_dict, user_guid);
					CFRelease(user_dict);
				}
			}

			// kDSNAttrRecordName
			value = ds_get_value(inUserID, user_dict, "pw_name", TRUE);
			if (value)
				strlcpy(in_out_opts->fRecName, value, sizeof in_out_opts->fRecName);
		} else if (user_data->count > 1)
			msg_error("od[getpwnam_ext]: multiple records (%u) found for user %s", user_data->count, inUserID);
		else if (!user_data->count)
			msg_error("od[getpwnam_ext]: no record found for user %s", inUserID);

		kvarray_free(user_data);
	} else if (errno)
		msg_error("od[getpwnam_ext]: unable to look up user record %s: %m", inUserID);
	else
		msg_error("od[getpwnam_ext]: no record for user %s", inUserID);

	if (POSTFIX_OD_LOOKUP_FINISH_ENABLED())
		POSTFIX_OD_LOOKUP_FINISH((char *) inUserID, in_out_opts, out_status);

	return out_status;
} /* ads_get_user_options */

/* ------------------------------------------------------------------
 *	aod_get_user_options ()
 */
int aod_get_user_options ( const char *inUserID, struct od_user_opts *in_out_opts )
{
	assert((inUserID != NULL) && (in_out_opts != NULL));

	memset( in_out_opts, 0, sizeof( struct od_user_opts ) );
	in_out_opts->fAcctState = eUnknownAcctState;

	if ( POSTFIX_OD_LOOKUP_START_ENABLED() )
		POSTFIX_OD_LOOKUP_START((char *) inUserID, in_out_opts);

	/* create default session */
	CFErrorRef cf_err_ref = NULL;
	ODSessionRef od_session_ref = ODSessionCreate( kCFAllocatorDefault, NULL, &cf_err_ref );
	if ( !od_session_ref ) {
		/* print the error and bail */
		print_cf_error( cf_err_ref, inUserID, "Unable to create OD Session" );
		return( -1 );
	}

	/* get seach node */
	ODNodeRef od_node_ref = ODNodeCreateWithNodeType( kCFAllocatorDefault, od_session_ref, kODNodeTypeAuthentication, &cf_err_ref );
	if ( !od_node_ref ) {
		/* print the error and bail */
		print_cf_error( cf_err_ref, inUserID, "Unable to create OD Node Reference" );

		/* release OD session */
		CFRelease( od_session_ref );
		return( -1 );
	}

	/* get account state and auto-forward address, if any */
	int out_status = get_user_attributes( od_node_ref, inUserID, in_out_opts );

	CFRelease( od_node_ref );
	CFRelease( od_session_ref );

	if ( POSTFIX_OD_LOOKUP_FINISH_ENABLED() )
		POSTFIX_OD_LOOKUP_FINISH((char *) inUserID, in_out_opts, out_status);

	return( out_status );
} /* aod_get_user_options */


/* -----------------------------------------------------------------
	Static functions
   ----------------------------------------------------------------- */

/* ------------------------------------------------------------------
 *	print_cf_error ()
 *
 *		print error returned in CFErrorRef
 */
static void print_cf_error ( CFErrorRef in_cf_err_ref, const char *in_user_name, const char *in_default_str )
{
	if ( in_cf_err_ref != NULL ) {
		CFStringRef cf_str_ref = CFErrorCopyFailureReason( in_cf_err_ref );
		if ( cf_str_ref != NULL ) {
			const char *err_str = CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingUTF8 );
			if ( err_str != NULL ) {
				syslog( LOG_ERR, "od: user %s: %s", in_user_name, err_str );
				CFRelease(cf_str_ref);
				return;
			}
			CFRelease(cf_str_ref);
		}
	}
	syslog( LOG_ERR, "od: user %s: %s", in_user_name, in_default_str );
} /* print_cf_error */

/* ------------------------------------------------------------------
 *	get_attr_from_record ()
 */
static CFStringRef get_attr_from_record ( ODRecordRef in_rec_ref, CFStringRef in_attr )
{
	CFErrorRef cf_err_ref = NULL;
	CFArrayRef cf_arry_values = ODRecordCopyValues( in_rec_ref, in_attr, &cf_err_ref );
	if ( !cf_arry_values )
		return( NULL );

	if ( CFArrayGetCount( cf_arry_values ) > 1 ) {
		msg_error( "aod: multiple attribute values (%d) found in record user record: %s for attribute: %s",
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
} /*  get_attr_from_record */

/* ------------------------------------------------------------------
 *	read_user_settings ()
 */
static CFPropertyListRef read_user_settings ( const char *in_file, CFPropertyListMutabilityOptions in_opts )
{
	CFPropertyListRef	cf_prop_list	= NULL;

	CFURLRef cf_url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)in_file, strlen(in_file), FALSE);
	if ( !cf_url ) {
		msg_error( "aod: could not create URL from %s", in_file);
		return( NULL );
	}

	SInt32 err;
	CFDataRef cf_data = NULL;
	if ( !CFURLCreateDataAndPropertiesFromResource(NULL, cf_url, &cf_data, NULL, NULL, &err) ) {
		msg_info("aod: no local user settings (%s), using defaults", in_file);
		CFRelease(cf_url);
		return( NULL );
	}

	CFStringRef cf_str_err = NULL;
	if ( cf_data ) {
		cf_prop_list = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cf_data, in_opts, &cf_str_err);
		if ( cf_prop_list )
			CFRetain(cf_prop_list);
	} else
		msg_error("aod: enable to create CFData ref for %s", in_file);

	CFRelease(cf_url);
	if ( cf_str_err )
		CFRelease( cf_str_err );
	if ( cf_data )
		CFRelease( cf_data );

	return( cf_prop_list );
} /* read_user_settings */

/* ------------------------------------------------------------------
 *	write_user_settings ()
 */
static void write_user_settings ( const char *in_file, CFPropertyListRef in_data )
{
	CFURLRef cf_url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)in_file, strlen(in_file), FALSE);
	if ( !cf_url ) {
		msg_error("aod: could not create URL from: %s", in_file);
		return;
	}

	CFDataRef cf_data = CFPropertyListCreateXMLData(NULL, in_data);
	if ( cf_data ) {
		SInt32 write_err = noErr;
		if ( !CFURLWriteDataAndPropertiesToResource(cf_url, cf_data, NULL, &write_err) )
			msg_error("aod: could not write to %s (error: %d)", in_file, (int) write_err);
		CFRelease(cf_data);
	}

	int fd = open(in_file, O_RDONLY);
	if (fd != -1) {
		fchown(fd, 27, 6);
		fchmod(fd, 0660);
		close(fd);
	}

	CFRelease(cf_url);
} /* write_user_settings */

/* ------------------------------------------------------------------
 *	get_attributes_local ()
 */
static bool get_attributes_local ( struct od_user_opts *in_out_opts, const char *in_user_guid )
{
	bool b_out = FALSE;

	/* look in local file first */
	CFDictionaryRef cf_dict_data = (CFDictionaryRef)read_user_settings( user_settings_path, kCFPropertyListImmutable );
	if ( !cf_dict_data )
		return( FALSE );

	CFStringRef cf_str = CFStringCreateWithCString( NULL, in_user_guid, kCFStringEncodingUTF8 );
	if ( cf_str ) {
		if ( CFDictionaryContainsKey( cf_dict_data, cf_str ) ) {
			CFDictionaryRef cf_dict_user = (CFDictionaryRef)CFDictionaryGetValue( cf_dict_data, cf_str );
			if ( cf_dict_user && (CFGetTypeID( cf_dict_user ) == CFDictionaryGetTypeID()) ) {
				get_acct_state( cf_dict_user, NULL, in_out_opts );
				get_alt_loc( cf_dict_user, NULL );
				get_mail_quota( cf_dict_user, NULL );
				b_out = TRUE;
			}
		}
		CFRelease( cf_str );
	}
	CFRelease( cf_dict_data );

	return( b_out );
} /* get_attributes_local */

/* ------------------------------------------------------------------
 *	set_attributes_local ()
 */
static void set_attributes_local ( CFMutableDictionaryRef in_user_dict, const char *in_user_guid )
{
	/* Get the file data */
	CFDictionaryRef cf_dict_data = (CFDictionaryRef)read_user_settings(user_settings_path, kCFPropertyListMutableContainers);
	if ( !cf_dict_data )
			cf_dict_data = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
								&kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if ( cf_dict_data && in_user_dict ) {
		CFStringRef cf_str = CFStringCreateWithCString( NULL, in_user_guid, kCFStringEncodingUTF8 );
		if ( cf_str != NULL ) {
			CFMutableDictionaryRef cf_mut_dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, cf_dict_data);
			if ( cf_mut_dict != NULL ) {
				CFDictionaryAddValue( cf_mut_dict, cf_str, in_user_dict );
				write_user_settings(user_settings_path, (CFPropertyListRef)cf_mut_dict );
				CFRelease(cf_mut_dict);
			}
			CFRelease(cf_str);
		}
	}
} /* set_attributes_local */

/* ------------------------------------------------------------------
 *	get_user_attributes ()
 */
static int get_user_attributes ( ODNodeRef in_node_ref, const char *in_user_name, struct od_user_opts *in_out_opts )
{
	CFStringRef cf_str_ref = CFStringCreateWithCString( NULL, in_user_name, kCFStringEncodingUTF8 );
	if ( !cf_str_ref ) {
		msg_error( "aod: unable to create user name CFStringRef");
		return( -1 );
	}

	/* look up user record */
	ODRecordRef od_rec_ref = NULL;
	CFErrorRef cf_err_ref = NULL;
	CFTypeRef cf_type_ref[] = { CFSTR(kDSAttributesStandardAll) };
	CFArrayRef cf_arry_ref = CFArrayCreate( NULL, cf_type_ref, 1, &kCFTypeArrayCallBacks );
	ODQueryRef cf_query_ref = ODQueryCreateWithNode( NULL, in_node_ref, CFSTR(kDSStdRecordTypeUsers), CFSTR(kDSNAttrRecordName),
											kODMatchInsensitiveEqualTo, cf_str_ref, cf_arry_ref, 100, &cf_err_ref );
	if ( cf_query_ref ) {
		CFArrayRef cf_arry_result = ODQueryCopyResults( cf_query_ref, false, &cf_err_ref );
		if ( cf_arry_result ) {
			if ( CFArrayGetCount( cf_arry_result ) == 1 ) {
				od_rec_ref = (ODRecordRef)CFArrayGetValueAtIndex( cf_arry_result, 0 );
				CFRetain(od_rec_ref);
			} else {
				if ( CFArrayGetCount( cf_arry_result ) == 0 )
					msg_error( "aod: no user record found for: %s", in_user_name );
				else
					msg_error( "aod: multiple user records (%ld) found for: %s", CFArrayGetCount( cf_arry_result ), in_user_name );
			}
			CFRelease(cf_arry_result);
		} else
			print_cf_error( cf_err_ref, in_user_name, "aod: ODQueryCopyResults() failed" );

		CFRelease( cf_query_ref );
	} else
		print_cf_error( cf_err_ref, in_user_name, "aod: ODQueryCreateWithNode() failed" );

	CFRelease( cf_str_ref );
	CFRelease( cf_arry_ref );

	if ( !od_rec_ref ) {
		/* print the error and bail */
		print_cf_error( cf_err_ref, in_user_name, "aod: unable to lookup user record" );
		return( -1 );
	}

	/* get guid */
	char user_guid[ 64 ];
	size_t str_size = 0;
	memset(user_guid, 0, sizeof user_guid);
	CFStringRef cf_str_value = get_attr_from_record( od_rec_ref, CFSTR(kDS1AttrGeneratedUID) );
	if ( cf_str_value ) {
		str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength( cf_str_value ), kCFStringEncodingUTF8) + 1;
		CFStringGetCString( cf_str_value, user_guid, str_size, kCFStringEncodingUTF8 );

		CFRelease( cf_str_value );
	}

	/* get record name */
	cf_str_value = ODRecordGetRecordName( od_rec_ref );
	if ( cf_str_value )
		str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength( cf_str_value ), kCFStringEncodingUTF8) + 1;
		if (str_size )
			CFStringGetCString( cf_str_value, in_out_opts->fRecName, sizeof(in_out_opts->fRecName), kCFStringEncodingUTF8 );

	/* get mail attribute */
	if ( !get_attributes_local(in_out_opts, user_guid) ) {
		cf_str_value = get_attr_from_record( od_rec_ref, CFSTR(kDS1AttrMailAttribute) );
		if ( cf_str_value != NULL ) {
			str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength( cf_str_value ), kCFStringEncodingUTF8) + 1;
			char *c_str = malloc( str_size );
			if ( c_str ) {
				if( CFStringGetCString( cf_str_value, c_str, str_size, kCFStringEncodingUTF8 ) ) {
					CFMutableDictionaryRef user_dict = get_mail_attribute_values( c_str, in_out_opts );
					set_attributes_local(user_dict, user_guid);
					CFRelease(user_dict);
				}
				free( c_str );
			}
			CFRelease( cf_str_value );
		}
	}

	CFRelease(od_rec_ref);

	return( 0 );
} /* get_user_attributes */

/* ------------------------------------------------------------------
 *	get_mail_attribute_values ()
 */
static CFMutableDictionaryRef get_mail_attribute_values ( const char *in_mail_attribute,
															struct od_user_opts *in_out_opts )
{
	CFMutableDictionaryRef cf_mut_dict_ref = NULL;
	unsigned long ul_size = strlen( in_mail_attribute );
	CFDataRef cf_data_ref = CFDataCreate( NULL, (const UInt8 *)in_mail_attribute, ul_size );
	if ( !cf_data_ref )
		return( NULL );

	CFPropertyListRef cf_plist_ref = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
										cf_data_ref, kCFPropertyListImmutable, NULL );
	if ( cf_plist_ref ) {
		if ( CFDictionaryGetTypeID() == CFGetTypeID( cf_plist_ref ) ) {
			cf_mut_dict_ref = CFDictionaryCreateMutable( kCFAllocatorDefault,
														0, &kCFCopyStringDictionaryKeyCallBacks,
														&kCFTypeDictionaryValueCallBacks);
			CFRetain(cf_mut_dict_ref);
			CFDictionaryAddValue( cf_mut_dict_ref, CFSTR(kXMLKeyAttrVersion), CFSTR(kXMLValueVersion2) );

			CFDictionaryRef cf_dict_ref = (CFDictionaryRef)cf_plist_ref;
			get_acct_state( cf_dict_ref, cf_mut_dict_ref, in_out_opts );
			get_alt_loc( cf_dict_ref, cf_mut_dict_ref );
			get_mail_quota( cf_dict_ref, cf_mut_dict_ref );
		}
	}
	CFRelease( cf_data_ref );

	return(cf_mut_dict_ref);
} /* get_mail_attribute_values */

/* ------------------------------------------------------------------
 *	get_acct_state ()
 */
void get_acct_state ( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict, struct od_user_opts *in_out_opts )
{
	/* enabled by default */
	in_out_opts->fAcctState = eAcctEnabled;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAcctState ) ) ) {
		CFStringRef cf_str_ref = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAcctState ) );
		if ( cf_str_ref ) {
			if ( CFGetTypeID( cf_str_ref ) == CFStringGetTypeID() ) {
				char *p_value = (char *)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
				if ( p_value ) {
					if ( strcasecmp( p_value, kXMLValueAcctEnabled ) == 0 )
						in_out_opts->fAcctState = eAcctEnabled;
					else if ( strcasecmp( p_value, kXMLValueAcctDisabled ) == 0 )
						in_out_opts->fAcctState = eAcctDisabled;
					else if ( strcasecmp( p_value, kXMLValueAcctFwd ) == 0 )
						get_auto_forward_addr( inCFDictRef, out_user_dict, in_out_opts );

					if ( out_user_dict )
						CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyAcctState), cf_str_ref);
				}
			}
		}
	}
} /* get_acct_state */

/* ------------------------------------------------------------------
 *	get_auto_forward_addr ()
 */
void get_auto_forward_addr ( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict, struct od_user_opts *in_out_opts )
{
	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) ) ) {
		CFStringRef cf_str_ref = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) );
		if ( cf_str_ref ) {
			if ( CFGetTypeID( cf_str_ref ) == CFStringGetTypeID() ) {
				char *p_value = (char *)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
				if ( p_value ) {
					in_out_opts->fAcctState = eAcctForwarded;
					strlcpy( in_out_opts->fAutoFwdAddr, p_value, sizeof(in_out_opts->fAutoFwdAddr) );

					if ( out_user_dict )
						CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyAutoFwd), cf_str_ref);
				}
			}
		}
	}
} /* get_auto_forward_addr */

/* ------------------------------------------------------------------
 *	get_alt_loc ()
 */
static void get_alt_loc ( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict )
{
	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAltDataStoreLoc ) ) ) {
		CFStringRef cf_str_ref = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAltDataStoreLoc ) );
		if ( cf_str_ref && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			if ( out_user_dict )
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyAltDataStoreLoc), cf_str_ref);
		}
	}
} /* get_alt_loc */

/* ------------------------------------------------------------------
 *	get_mail_quota ()
 */
static void get_mail_quota ( CFDictionaryRef inCFDictRef, CFMutableDictionaryRef out_user_dict )
{
	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyDiskQuota ) ) ) {
		CFStringRef cf_str_ref = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyDiskQuota ) );
		if ( cf_str_ref && (CFGetTypeID( cf_str_ref ) == CFStringGetTypeID()) ) {
			if ( out_user_dict )
				CFDictionaryAddValue( out_user_dict, CFSTR(kXMLKeyDiskQuota), cf_str_ref);
		}
	}
} /* get_mail_quota */


int sacl_check(const char *inUserID)
{
	int err, result;
	uuid_t guid;

	if (POSTFIX_SACL_START_ENABLED())
		POSTFIX_SACL_START((char *) inUserID);

	if (var_use_sacl_cache) {
		/* look up in cache */
		result = SACL_CHECK_STATUS_UNKNOWN;
		switch (sacl_cache_clnt_get(inUserID, &result)) {
		case SACL_CACHE_STAT_OK:
			if (result == SACL_CHECK_STATUS_AUTHORIZED) {
				if (POSTFIX_SACL_CACHED_ENABLED())
					POSTFIX_SACL_CACHED((char *) inUserID, 1);
				return 1;
			} else if (result == SACL_CHECK_STATUS_UNAUTHORIZED) {
				if (POSTFIX_SACL_CACHED_ENABLED())
					POSTFIX_SACL_CACHED((char *) inUserID, 0);
				return 0;
			} else if (result == SACL_CHECK_STATUS_NO_SACL) {
				if (POSTFIX_SACL_CACHED_ENABLED())
					POSTFIX_SACL_CACHED((char *) inUserID, -1);
				return 1;
			}
			break;
		case SACL_CACHE_STAT_BAD:
			msg_warn("sacl_check: %s protocol error", var_sacl_cache_service);
			break;
		case SACL_CACHE_STAT_FAIL:
			msg_warn("sacl_check: %s service failure", var_sacl_cache_service);
			break;
		}
	}

	/* cache miss; perform SACL check */
	err = mbr_user_name_to_uuid(inUserID, guid);
	if (err) {
		if (POSTFIX_SACL_RESOLVE_ENABLED())
			POSTFIX_SACL_RESOLVE((char *) inUserID, 0);
		msg_info("sacl_check: mbr_user_name_to_uuid(%s) failed: %s",
				 inUserID, strerror(err));
		return -1;
	}

	if (POSTFIX_SACL_RESOLVE_ENABLED())
		POSTFIX_SACL_RESOLVE((char *) inUserID, 1);

	result = 0;
	err = mbr_check_service_membership(guid, "mail", &result);
	if (err) {
		if (POSTFIX_SACL_FINISH_ENABLED())
			POSTFIX_SACL_FINISH((char *) inUserID, -1);

		if (err != ENOENT) {
			msg_error("sacl_check: mbr_check_service_membership(%s, mail) failed: %s",
					  inUserID, strerror(err));
			return -1;
		}

		if (var_use_sacl_cache) {
			/* mail SACL is off.  tell cache */
			(void) sacl_cache_clnt_no_sacl();
		}
		return 1;
	}

	if (POSTFIX_SACL_FINISH_ENABLED())
		POSTFIX_SACL_FINISH((char *) inUserID, result);

	if (var_use_sacl_cache) {
		/* update cache */
		switch (sacl_cache_clnt_put(inUserID, result ?
					    SACL_CHECK_STATUS_AUTHORIZED :
					    SACL_CHECK_STATUS_UNAUTHORIZED)) {
		case SACL_CACHE_STAT_OK:
			break;
		case SACL_CACHE_STAT_BAD:
			msg_warn("sacl_check: %s protocol error", var_sacl_cache_service);
			break;
		case SACL_CACHE_STAT_FAIL:
			msg_warn("sacl_check: %s service failure", var_sacl_cache_service);
			break;
		}
	}

	return result;
}
