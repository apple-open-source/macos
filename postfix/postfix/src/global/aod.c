/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include "aod.h"

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

static void			print_cf_error				( CFErrorRef in_cf_err_ref, const char *in_default_str );
static int			get_user_attributes			( ODNodeRef in_node_ref, const char *in_user_name, struct od_user_opts *in_out_opts );
static void			get_mail_attribute_values	( const char *in_mail_attribute, struct od_user_opts *in_out_opts );
static void			get_auto_forward_addr		( CFDictionaryRef inCFDictRef, struct od_user_opts *in_out_opts );
void				get_acct_state				( CFDictionaryRef inCFDictRef, struct od_user_opts *in_out_opts );
static CFStringRef	get_attr_from_record		( ODRecordRef in_rec_ref, CFStringRef in_attr );

/* Begin DS SPI Glue */
#include <kvbuf.h>
#include <DSlibinfoMIG.h>
#include <DirectoryService/DirectoryService.h>

extern mach_port_t _ds_port;
extern int _ds_running();

__private_extern__ kern_return_t
get_procno( const char *procname, int32_t *procno )
{
	kern_return_t		status;
	security_token_t	token;
	bool				lookAgain;
	uid_t				uid;

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
	uid_t					uid;
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

static const char *ds_get_value(const char *inUserID, const kvdict_t *in_dict, const char *in_attr, bool first_of_many)
{
	const char *value = NULL;
	int32_t i;

	for (i = 0; i < in_dict->kcount; i++) {
		if (!strcmp(in_dict->key[i], in_attr)) {
			if (in_dict->vcount[i] == 1)
				value = in_dict->val[i][0];
			else if (in_dict->vcount[i] == 0)
				msg_info("od[getpwnam_ext]: no value found for attribute %s in record for user %s", in_attr, inUserID);
			else if (first_of_many) {
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
	if (i >= in_dict->kcount)
		msg_info("od[getpwnam_ext]: no attribute %s in record for user %s", in_attr, inUserID);

	return value;
}

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
		} else if (user_data->count == 0)
			msg_error("od[getpwnam_ext]: no record found for user %s", inUserID);
		else
			msg_error("od[getpwnam_ext]: multiple records (%u) found for user %s", user_data->count, inUserID);

		kvarray_free(user_data);
	} else if (errno)
		msg_error("od[getpwnam_ext]: Unable to look up user record %s: %m", inUserID);

	if (strlen(rec_name))
		return(rec_name);
	return( NULL );
}

int ads_get_user_options(const char *inUserID, struct od_user_opts *in_out_opts)
{
	int out_status = 0;
	kvarray_t *user_data;

	assert(inUserID != NULL && in_out_opts != NULL);
	memset(in_out_opts, 0, sizeof *in_out_opts);
	in_out_opts->fAcctState = eUnknownAcctState;

	if (POSTFIX_OD_LOOKUP_START_ENABLED())
		POSTFIX_OD_LOOKUP_START((char *) inUserID, in_out_opts);

	errno = 0;
	user_data = getpwnam_ext(inUserID);
	if (user_data != NULL) {
		if (user_data->count == 1) {
			kvdict_t *user_dict = &user_data->dict[0];
			const char *value;

			value = ds_get_value(inUserID, user_dict, kDS1AttrMailAttribute, FALSE);
			if (value)
				get_mail_attribute_values(value, in_out_opts);

			// kDSNAttrRecordName
			value = ds_get_value(inUserID, user_dict, "pw_name", TRUE);
			if (value)
				strlcpy(in_out_opts->fRecName, value, sizeof in_out_opts->fRecName);
		} else if (user_data->count == 0)
			msg_error("od[getpwnam_ext]: no record found for user %s", inUserID);
		else
			msg_error("od[getpwnam_ext]: multiple records (%u) found for user %s", user_data->count, inUserID);

		kvarray_free(user_data);
	} else if (errno)
		msg_error("od[getpwnam_ext]: Unable to look up user record %s: %m", inUserID);
	else
		msg_error("od[getpwnam_ext]: No record for user %s", inUserID);

	if (POSTFIX_OD_LOOKUP_FINISH_ENABLED())
		POSTFIX_OD_LOOKUP_FINISH((char *) inUserID, in_out_opts, out_status);

	return out_status;
}

/* -----------------------------------------------------------------
	aod_get_user_options ()
   ----------------------------------------------------------------- */

int aod_get_user_options ( const char *inUserID, struct od_user_opts *in_out_opts )
{
	int					out_status		= 0;
	ODSessionRef		od_session_ref;
	ODNodeRef			od_node_ref;
	CFErrorRef			cf_err_ref		= NULL;

	assert((inUserID != NULL) && (in_out_opts != NULL));

	memset( in_out_opts, 0, sizeof( struct od_user_opts ) );

	in_out_opts->fAcctState = eUnknownAcctState;

	if (POSTFIX_OD_LOOKUP_START_ENABLED())
		POSTFIX_OD_LOOKUP_START((char *) inUserID, in_out_opts);

	// create default session
	od_session_ref = ODSessionCreate( kCFAllocatorDefault, NULL, &cf_err_ref );
	if ( od_session_ref == NULL )
	{
		/* print the error and bail */
		print_cf_error( cf_err_ref, "Unable to create OD Session" );
		return( -1 );
	}

	// get seach node
	od_node_ref = ODNodeCreateWithNodeType( kCFAllocatorDefault, od_session_ref, kODNodeTypeAuthentication, &cf_err_ref );
	if ( od_node_ref == NULL )
	{
		/* print the error and bail */
		print_cf_error( cf_err_ref, "Unable to create OD Node Reference" );

		/* release OD session */
		CFRelease( od_session_ref );
		od_session_ref = NULL;
		return( -1 );
	}

	/* get account state and auto-forward address, if any */
	out_status = get_user_attributes( od_node_ref, inUserID, in_out_opts );

	CFRelease( od_node_ref );
	CFRelease( od_session_ref );

	if (POSTFIX_OD_LOOKUP_FINISH_ENABLED())
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
 * ------------------------------------------------------------------*/

static void print_cf_error ( CFErrorRef in_cf_err_ref, const char *in_default_str )
{
	CFStringRef		cf_str_ref;

	if ( in_cf_err_ref != NULL )
	{
		cf_str_ref = CFErrorCopyFailureReason( in_cf_err_ref );
		if ( cf_str_ref != NULL )
		{
			const char *err_str = CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingUTF8 );
			if ( err_str != NULL )
			{
				syslog( LOG_ERR, "od: %s", err_str );
				CFRelease(cf_str_ref);
				return;
			}
			CFRelease(cf_str_ref);
		}
	}

	syslog( LOG_ERR, "od: %s", in_default_str );
} /* print_cf_error */


/* ------------------------------------------------------------------
 *	get_attr_from_record ()
 * ------------------------------------------------------------------*/

static CFStringRef get_attr_from_record ( ODRecordRef in_rec_ref, CFStringRef in_attr )
{
	CFArrayRef		cf_arry_values	= NULL;
	CFErrorRef		cf_err_ref		= NULL;
	CFStringRef		cf_str_out		= NULL;

	cf_arry_values = ODRecordCopyValues( in_rec_ref, in_attr, &cf_err_ref );
	if ( cf_arry_values == NULL )
	{
		return( NULL );
	}

	if ( CFArrayGetCount( cf_arry_values ) > 1 )
	{
		msg_error( "aod: multiple attribute values (%d) found in record user record: %s for attribute: %s",
					(int)CFArrayGetCount( cf_arry_values ),
					CFStringGetCStringPtr( ODRecordGetRecordName( in_rec_ref ), kCFStringEncodingUTF8 ),
					CFStringGetCStringPtr( in_attr, kCFStringEncodingUTF8 ) );
		CFRelease( cf_arry_values );
		return( NULL );
	}

	cf_str_out = CFArrayGetValueAtIndex( cf_arry_values, 0 );
	CFRetain( cf_str_out );

	CFRelease( cf_arry_values );

	return( cf_str_out );
} /*  get_attr_from_record */


/* -----------------------------------------------------------------
	get_user_attributes ()
   ----------------------------------------------------------------- */

static int get_user_attributes ( ODNodeRef in_node_ref, const char *in_user_name, struct od_user_opts *in_out_opts )
{
	char		   *c_str			= NULL;
	size_t			str_size		= 0;
	ODQueryRef		cf_query_ref	= NULL;
	ODRecordRef		od_rec_ref		= NULL;
	CFStringRef		cf_str_ref		= NULL;
	CFStringRef		cf_str_value	= NULL;
	CFTypeRef		cf_type_ref[]	= { CFSTR(kDSAttributesStandardAll) };
	CFArrayRef		cf_arry_ref		= CFArrayCreate( NULL, cf_type_ref, 1, &kCFTypeArrayCallBacks );
	CFArrayRef		cf_arry_result	= NULL;
	CFErrorRef		cf_err_ref		= NULL;

	cf_str_ref = CFStringCreateWithCString( NULL, in_user_name, kCFStringEncodingUTF8 );
	if ( cf_str_ref == NULL )
	{
		msg_error( "aod: unable to create user name CFStringRef");
		CFRelease( cf_arry_ref );
		return( -1 );
	}

	/* look up user record */
	cf_query_ref = ODQueryCreateWithNode( NULL, in_node_ref, CFSTR(kDSStdRecordTypeUsers), CFSTR(kDSNAttrRecordName),
											kODMatchInsensitiveEqualTo, cf_str_ref, cf_arry_ref, 100, &cf_err_ref );
	if ( cf_query_ref )
	{
		cf_arry_result = ODQueryCopyResults( cf_query_ref, false, &cf_err_ref );
		if ( cf_arry_result )
		{
			if ( CFArrayGetCount( cf_arry_result ) == 1 )
			{
				od_rec_ref = (ODRecordRef)CFArrayGetValueAtIndex( cf_arry_result, 0 );
				CFRetain(od_rec_ref);
			}
			else
			{
				if ( CFArrayGetCount( cf_arry_result ) == 0 )
				{
					msg_error( "aod: no user record found for: %s", in_user_name );
				}
				else
				{
					msg_error( "aod: multiple user records (%ld) found for: %s", CFArrayGetCount( cf_arry_result ), in_user_name );
				}
			}
			CFRelease(cf_arry_result);
		}
		else
		{
			print_cf_error( cf_err_ref, "aod: OD Query Copy Results failed" );
		}
		CFRelease( cf_query_ref );
	}
	else
	{
		print_cf_error( cf_err_ref, "aod: OD Query Create With Node failed" );
	}

	CFRelease( cf_str_ref );
	CFRelease( cf_arry_ref );

	if ( od_rec_ref == NULL )
	{
		/* print the error and bail */
		print_cf_error( cf_err_ref, "aod: Unable to lookup user record" );
		return( -1 );
	}

	/* get mail attribute */
	cf_str_value = get_attr_from_record( od_rec_ref, CFSTR(kDS1AttrMailAttribute) );
	if ( cf_str_value != NULL )
	{
		str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength( cf_str_value ), kCFStringEncodingUTF8) + 1;
		c_str = malloc( str_size );
		if ( c_str != NULL )
		{
			if( CFStringGetCString( cf_str_value, c_str, str_size, kCFStringEncodingUTF8 ) )
			{
				get_mail_attribute_values( c_str, in_out_opts );
			}
			free( c_str );
		}
		CFRelease( cf_str_value );
	}

	/* get record name */
	cf_str_value = ODRecordGetRecordName( od_rec_ref );
	if ( cf_str_value != NULL )
		str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength( cf_str_value ), kCFStringEncodingUTF8) + 1;
		if (str_size )
			CFStringGetCString( cf_str_value, in_out_opts->fRecName, sizeof(in_out_opts->fRecName), kCFStringEncodingUTF8 );

	CFRelease(od_rec_ref);

	return( 0 );
} /* get_user_attributes */


/* ------------------------------------------------------------------
 *	get_mail_attribute_values ()
 * ------------------------------------------------------------------*/

static void get_mail_attribute_values ( const char *in_mail_attribute, struct od_user_opts *in_out_opts )
{
	unsigned long		ul_size			= 0;
	CFDataRef			cf_data_ref		= NULL;
	CFPropertyListRef	cf_plist_ref	= NULL;
	CFDictionaryRef		cf_dict_ref		= NULL;

	ul_size = strlen( in_mail_attribute );
	cf_data_ref = CFDataCreate( NULL, (const UInt8 *)in_mail_attribute, ul_size );
	if ( cf_data_ref != NULL )
	{
		cf_plist_ref = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cf_data_ref, kCFPropertyListImmutable, NULL );
		if ( cf_plist_ref != NULL )
		{
			if ( CFDictionaryGetTypeID() == CFGetTypeID( cf_plist_ref ) )
			{
				cf_dict_ref = (CFDictionaryRef)cf_plist_ref;
				get_acct_state( cf_dict_ref, in_out_opts );
			}
			CFRelease( cf_plist_ref );
		}
		CFRelease( cf_data_ref );
	}
} /* get_mail_attribute_values */


/* -----------------------------------------------------------------
	get_acct_state ()
   ----------------------------------------------------------------- */

void get_acct_state ( CFDictionaryRef inCFDictRef, struct od_user_opts *in_out_opts )
{
	char		   *p_value		= NULL;
	CFStringRef		cf_str_ref	= NULL;

	/* Default value */
	in_out_opts->fAcctState = eUnknownAcctState;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAcctState ) ) )
	{
		cf_str_ref = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAcctState ) );
		if ( cf_str_ref != NULL )
		{
			if ( CFGetTypeID( cf_str_ref ) == CFStringGetTypeID() )
			{
				p_value = (char *)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
				if ( p_value != NULL )
				{
					if ( strcasecmp( p_value, kXMLValueAcctEnabled ) == 0 )
					{
						in_out_opts->fAcctState = eAcctEnabled;
					}
					else if ( strcasecmp( p_value, kXMLValueAcctDisabled ) == 0 )
					{
						in_out_opts->fAcctState = eAcctDisabled;
					}
					else if ( strcasecmp( p_value, kXMLValueAcctFwd ) == 0 )
					{
						get_auto_forward_addr( inCFDictRef, in_out_opts );
					}
				}
			}
		}
	}
} /* get_acct_state */


/* -----------------------------------------------------------------
	get_auto_forward_addr ()
   ----------------------------------------------------------------- */

void get_auto_forward_addr ( CFDictionaryRef inCFDictRef, struct od_user_opts *in_out_opts )
{
	char		   *p_value		= NULL;
	CFStringRef		cf_str_ref	= NULL;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) ) )
	{
		cf_str_ref = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) );
		if ( cf_str_ref != NULL )
		{
			if ( CFGetTypeID( cf_str_ref ) == CFStringGetTypeID() )
			{
				p_value = (char *)CFStringGetCStringPtr( cf_str_ref, kCFStringEncodingMacRoman );
				if ( p_value != NULL )
				{
					in_out_opts->fAcctState = eAcctForwarded;
					strlcpy( in_out_opts->fAutoFwdAddr, p_value, sizeof(in_out_opts->fAutoFwdAddr) );
				}
			}
		}
	}
} /* get_auto_forward_addr */
