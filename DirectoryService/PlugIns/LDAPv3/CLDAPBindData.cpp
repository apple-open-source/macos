/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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

#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint
#include <syslog.h>
#include <arpa/inet.h>
#include <spawn.h>
#include <grp.h>
#include <CommonCrypto/CommonDigest.h>

#include <Security/Authorization.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>
#include <CoreFoundation/CFPriv.h>
#include <PasswordServer/PSUtilitiesDefs.h>
#include <PasswordServer/KerberosInterface.h>
#include <PasswordServer/KerberosServiceSetup.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
#include "DirServicesConstPriv.h"
#include "DirServicesPriv.h"
#include "CLDAPPlugInPrefs.h"
#include "LDAPv3SupportFunctions.h"

#include "CLDAPBindData.h"
#include "PrivateTypes.h"
#include "DSLDAPUtils.h"

CLDAPBindData::CLDAPBindData( tDataBufferPtr inXMLData, CFMutableDictionaryRef *outXMLDict ) :
	mServer(NULL),
	mServerCFString(NULL),
	mSSL(false),
	mLDAPv2Only(false),
	mUserName(NULL),
	mPassword(NULL),
	mCFComputerMap(NULL),
	mComputerName(NULL),
	mEnetAddr(NULL)
{
	UInt32 iLength = 0;
	
	if ( outXMLDict != NULL )
		*outXMLDict = NULL;
	
	if ( inXMLData == NULL )
		return;
	
	// we should always have XML data for this process, if we don't, throw an error
	CFMutableDictionaryRef cfXMLDict = GetXMLFromBuffer( inXMLData );
	if ( cfXMLDict == NULL )
		return;
	
	*outXMLDict = cfXMLDict;
	
	// pluck out the dictionary values
	
	CFStringRef cfServer = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLServerKey) );
	if ( cfServer != NULL && CFGetTypeID(cfServer) == CFStringGetTypeID() )
	{
		CFRetain( cfServer );
		mServerCFString = cfServer;
		
		iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfServer), kCFStringEncodingUTF8) + 1;
		mServer = (char *) calloc( sizeof(char), iLength );
		if ( mServer != NULL )
			CFStringGetCString( cfServer, mServer, iLength, kCFStringEncodingUTF8 );
	}
	
	CFBooleanRef cfBool = (CFBooleanRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLIsSSLFlagKey) );
	if ( cfBool != NULL && CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
		mSSL = CFBooleanGetValue( cfBool );
	
	cfBool = (CFBooleanRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLLDAPv2ReadOnlyKey) );
	if ( cfBool != NULL && CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
		mLDAPv2Only = CFBooleanGetValue( cfBool );

	CFStringRef cfUsername = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLServerAccountKey) );
	if ( cfUsername != NULL && CFGetTypeID(cfUsername) == CFStringGetTypeID() && CFStringGetLength(cfUsername) != 0 )
	{
		iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfUsername), kCFStringEncodingUTF8 ) + 1;
		mUserName = (char *) calloc( sizeof(char), iLength );
		if ( mUserName != NULL )
			CFStringGetCString( cfUsername, mUserName, iLength, kCFStringEncodingUTF8 );
	}
	
	CFStringRef cfPassword = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLServerPasswordKey) );
	if ( cfPassword != NULL && CFGetTypeID(cfPassword) == CFStringGetTypeID() && CFStringGetLength(cfPassword) != 0 )
	{
		iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfPassword), kCFStringEncodingUTF8) + 1;
		mPassword = (char *) calloc( sizeof(char), iLength );
		if ( mPassword != NULL )
			CFStringGetCString( cfPassword, mPassword, iLength, kCFStringEncodingUTF8 );
	}
	
	mCFComputerMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeComputers) );
	
	CFStringRef cfComputerName = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLUserDefinedNameKey) );
	if ( cfComputerName != NULL && CFGetTypeID(cfComputerName) == CFStringGetTypeID() )
	{
		iLength = (UInt32)CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfComputerName), kCFStringEncodingUTF8) + 1;
		mComputerName = (char *) calloc( sizeof(char), iLength );
		if ( mComputerName != NULL )
			CFStringGetCString( cfComputerName, mComputerName, iLength, kCFStringEncodingUTF8 );
	}
	
	CFStringRef cfEnetAddr = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kDS1AttrENetAddress) );
	if ( cfEnetAddr != NULL && CFGetTypeID(cfEnetAddr) == CFStringGetTypeID() )
	{
		iLength = (UInt32)CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfEnetAddr), kCFStringEncodingUTF8 ) + 1;
		mEnetAddr = (char *)calloc( sizeof(char), iLength );
		if ( mEnetAddr != NULL )
			CFStringGetCString( cfEnetAddr, mEnetAddr, iLength, kCFStringEncodingUTF8 );
	}
}


CLDAPBindData::~CLDAPBindData( void )
{
	DSFreeString( mServer );
	DSCFRelease( mServerCFString );
	DSFreeString( mUserName );
	DSFreePassword( mPassword );
	DSCFRelease( mCFComputerMap );
	DSFreeString( mComputerName );
	DSFreeString( mEnetAddr );
}


// ---------------------------------------------------------------------------
//	* DataValidForBind
// ---------------------------------------------------------------------------

tDirStatus
CLDAPBindData::DataValidForBind( void )
{
	if ( mServerCFString == NULL || CFGetTypeID(mServerCFString) != CFStringGetTypeID() )
		return eDSInvalidBuffFormat;
	
	if ( mCFComputerMap == NULL )
		return eDSNoStdMappingAvailable;
	
	if ( mComputerName == NULL )
		return eDSInvalidRecordName;
	
	return eDSNoErr;
}


// ---------------------------------------------------------------------------
//	* DataValidForRemove
// ---------------------------------------------------------------------------

tDirStatus
CLDAPBindData::DataValidForRemove( void )
{
	if ( mServerCFString == NULL || CFGetTypeID(mServerCFString) != CFStringGetTypeID() )
		return eDSInvalidBuffFormat;
	
	return eDSNoErr;
}


// ---------------------------------------------------------------------------
//	* CreateMappingFromConfig
// ---------------------------------------------------------------------------

CFDictionaryRef
CLDAPBindData::CreateMappingFromConfig( CFDictionaryRef inDict, CFStringRef inRecordType )
{
	CFArrayRef				cfRecordMap		= (CFArrayRef) CFDictionaryGetValue( inDict, CFSTR(kXMLRecordTypeMapArrayKey) );
	CFMutableDictionaryRef  cfReturnDict	= NULL;
	
	if ( cfRecordMap != NULL && CFGetTypeID(cfRecordMap) == CFArrayGetTypeID() )
	{
		CFIndex					iCount			= CFArrayGetCount( cfRecordMap );
		CFDictionaryRef			cfRecordDict	= NULL;
		
		for( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfRecordMap, ii );
			
			if ( CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
			{
				CFStringRef cfMapName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
				
				if ( cfMapName != NULL && CFStringCompare( cfMapName, inRecordType, 0) == kCFCompareEqualTo )
				{
					cfRecordDict = cfMapDict;
					break;
				}
			}
		}
		
		if ( cfRecordDict != NULL )
		{
			// Now let's read the newly found map and find out where to look for config records..
			CFArrayRef cfNativeMap = (CFArrayRef) CFDictionaryGetValue( cfRecordDict, CFSTR(kXMLNativeMapArrayKey) );
			
			if ( cfNativeMap != NULL && CFGetTypeID(cfNativeMap) == CFArrayGetTypeID() )
			{
				if ( CFArrayGetCount(cfNativeMap) > 0 )
				{
					// let's assume we have mappings at this point...
					cfReturnDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
					
					CFDictionaryRef cfNativeDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfNativeMap, 0 );
					if ( cfNativeDict != NULL && CFGetTypeID(cfNativeDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfSearchbase = (CFStringRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLSearchBase) );
						if ( cfSearchbase != NULL && CFGetTypeID(cfSearchbase) == CFStringGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLSearchBase), cfSearchbase );
						
						CFStringRef cfGroupStyle = (CFStringRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLGroupObjectClasses) );
						if ( cfGroupStyle != NULL && CFGetTypeID(cfGroupStyle) == CFStringGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLGroupObjectClasses), cfGroupStyle );
						
						CFArrayRef cfObjectClasses = (CFArrayRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLObjectClasses) );
						if ( cfObjectClasses != NULL && CFGetTypeID(cfObjectClasses) == CFArrayGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLObjectClasses), cfObjectClasses );
					}
				}
			}
			
			// Now let's read the attribute map
			CFArrayRef cfAttribMap = (CFArrayRef) CFDictionaryGetValue( cfRecordDict, CFSTR(kXMLAttrTypeMapArrayKey) );
			if ( cfAttribMap != NULL && CFGetTypeID(cfAttribMap) == CFArrayGetTypeID() )
			{
				CFMutableDictionaryRef  cfAttribDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				
				CFDictionarySetValue( cfReturnDict, CFSTR("Attributes"), cfAttribDict );
				CFRelease( cfAttribDict ); // since we just added it to the dictionary we can release it now.
				
				iCount = CFArrayGetCount( cfAttribMap );
				for( CFIndex ii = 0; ii < iCount; ii++ )
				{
					CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfAttribMap, ii );
					if ( cfMapDict != NULL && CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
						
						if ( cfName != NULL && CFGetTypeID(cfName) == CFStringGetTypeID())
						{
							CFArrayRef cfNativeMapTemp = (CFArrayRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLNativeMapArrayKey) );
							
							if ( cfNativeMap != NULL && CFGetTypeID(cfNativeMapTemp) == CFArrayGetTypeID() )
							{
								// set the key to the name and array to the value
								CFDictionarySetValue( cfAttribDict, cfName, cfNativeMapTemp );
							}
						}
					}
				}
			}
			
			// now let's go through the Attribute Type Map and see if there are any mappings we don't already have
			cfAttribMap = (CFArrayRef) CFDictionaryGetValue( inDict, CFSTR(kXMLAttrTypeMapArrayKey) );
			if ( cfAttribMap != NULL && CFGetTypeID(cfAttribMap) == CFArrayGetTypeID() )
			{
				CFMutableDictionaryRef  cfAttribDict = (CFMutableDictionaryRef) CFDictionaryGetValue( cfReturnDict, CFSTR("Attributes") );
				if ( cfAttribDict == NULL )
				{
					cfAttribDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					CFDictionarySetValue( cfReturnDict, CFSTR("Attributes"), cfAttribDict );
					CFRelease( cfAttribDict ); // since we just added it to the dictionary we can release it now.
				}
				
				iCount = CFArrayGetCount( cfAttribMap );
				for( CFIndex ii = 0; ii < iCount; ii++ )
				{
					CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfAttribMap, ii );
					if ( cfMapDict != NULL && CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
						
						if ( cfName != NULL && CFGetTypeID(cfName) == CFStringGetTypeID())
						{
							CFArrayRef cfNativeMapTemp = (CFArrayRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLNativeMapArrayKey) );
							
							if ( cfNativeMapTemp != NULL && CFGetTypeID(cfNativeMapTemp) == CFArrayGetTypeID() )
							{
								// if the key doesn't already exist, let's go ahead and put the high level one in there..
								// it's not additive, only if it is missing
								if ( CFDictionaryGetValue(cfAttribDict, cfName) == NULL )
								{
									// set the key to the name and array to the value
									CFDictionarySetValue( cfAttribDict, cfName, cfNativeMapTemp );
								}
							}
						}
					}
				}
			}
		}
	}
	
	return cfReturnDict;
} // CreateMappingFromConfig


const char*
CLDAPBindData::Server( void )
{
	return mServer;
}


CFStringRef
CLDAPBindData::ServerCFString( void )
{
	return mServerCFString;
}


bool
CLDAPBindData::SSL( void )
{
	return mSSL;
}


bool
CLDAPBindData::LDAPv2ReadOnly( void )
{
	return mLDAPv2Only;
}


const char*
CLDAPBindData::UserName( void )
{
	return mUserName;
}


const char*
CLDAPBindData::Password( void )
{
	return mPassword;
}


CFDictionaryRef
CLDAPBindData::ComputerMap( void )
{
	return mCFComputerMap;
}


const char*
CLDAPBindData::ComputerName( void )
{
	return mComputerName;
}


const char*
CLDAPBindData::EnetAddress( void )
{
	return mEnetAddr;
}

