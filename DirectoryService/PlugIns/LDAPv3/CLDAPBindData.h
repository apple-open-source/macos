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

#ifndef __CLDAPBINDDATA__
#define __CLDAPBINDDATA__

#include <CoreFoundation/CoreFoundation.h>
#include "DirServicesTypes.h"

class CLDAPBindData
{
	public:
											CLDAPBindData( tDataBufferPtr inXMLData, CFMutableDictionaryRef *outXMLDict );
		virtual								~CLDAPBindData( void );
		
		virtual tDirStatus					DataValidForBind( void );
		virtual tDirStatus					DataValidForRemove( void );
		
		CFDictionaryRef						CreateMappingFromConfig( CFDictionaryRef inDict, CFStringRef inRecordType );
		
		void								SecureUseFlag( bool inSecureUse );
		
		// accessors
		const char*							Server( void );
		CFStringRef							ServerCFString( void );
		bool								SSL( void );
		bool								LDAPv2ReadOnly( void );
		const char*							UserName( void );
		const char*							Password( void );
		CFDictionaryRef						ComputerMap( void );
		const char*							ComputerName( void );
		const char*							EnetAddress( void );
		
	protected:
		char *mServer;
		CFStringRef mServerCFString;
		bool mSSL;
		bool mLDAPv2Only;
		char *mUserName;
		char *mPassword;
		CFDictionaryRef mCFComputerMap;
		char *mComputerName;
		char *mEnetAddr;
};


#endif
