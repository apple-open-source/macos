/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#ifdef __MWERKS__
#define _KC_UTILITIES
#endif

#include <Security/KCUtilities.h>
#include <Security/cssmerrno.h>

namespace Security
{

OSStatus GetKeychainErrFromCSSMErr( OSStatus cssmError )
{
	if (CSSM_ERR_IS_CONVERTIBLE(cssmError))
	{
		switch (CSSM_ERRCODE(cssmError))
		{
			// CONVERTIBLE ERROR CODES.
			case CSSM_ERRCODE_SERVICE_NOT_AVAILABLE:
				return errSecNotAvailable;
			case CSSM_ERRCODE_USER_CANCELED:
				return userCanceledErr;
			case CSSM_ERRCODE_OPERATION_AUTH_DENIED:
				return errSecAuthFailed;
			default:
				return cssmError;
		}
	}
	else
	{
		switch (cssmError)
		{
			// DL SPECIFIC ERROR CODES
			case CSSMERR_DL_RECORD_NOT_FOUND:
				return errSecItemNotFound;
			case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
				return errSecDuplicateItem;
			case CSSMERR_DL_DATABASE_CORRUPT:
				return errSecInvalidKeychain;
			case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
				return errSecNoSuchKeychain;
			case CSSMERR_DL_DATASTORE_ALREADY_EXISTS:
				return errSecDuplicateKeychain;
			case CSSMERR_DL_INVALID_FIELD_NAME:
				return errSecNoSuchAttr;
			default:
				return cssmError;
		}
	}
}

StKCAttribute::StKCAttribute( SecKeychainAttribute* inPtr ) :
    fAttr( inPtr )
{
}

StKCAttribute::~StKCAttribute( )
{
    delete fAttr;
}

StKCItem::StKCItem( SecKeychainItemRef* inItem, OSStatus* result ) :
    fItem( inItem ),
    fResult( result )
{
}

StKCItem::~StKCItem( )
{
    // if an error occured and the item is valid, release the item
    //
    if ( *fResult != noErr && *fItem != NULL )
        ::SecKeychainItemRelease(*fItem ); // %%% rjp was KCItemRelease(fitem);
}

} // end namespace Security
