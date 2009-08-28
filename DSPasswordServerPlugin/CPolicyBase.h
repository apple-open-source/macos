/*
 *  CPolicyBase.h
 *  PasswordServerPlugin
 *
 *  Created by Administrator on Fri Nov 21 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef __CPOLICYBASE__
#define __CPOLICYBASE__

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <PasswordServer/AuthFile.h>

#ifdef __cplusplus
};
#endif

class CPolicyBase
{
	public:
	
												CPolicyBase();
												CPolicyBase( CFDictionaryRef inPolicyDict );
												CPolicyBase( const char *xmlDataStr );
		virtual									~CPolicyBase();
		
		virtual void							CPolicyCommonInit( void ) = 0;
		virtual char *							GetPolicyAsSpaceDelimitedData( void ) = 0;
		virtual char *							GetPolicyAsXMLData( void );
		
        static bool								ConvertCFDateToBSDTime( CFDateRef inDateRef, struct tm *outBSDDate );
        static bool								ConvertCFDateToBSDTime( CFDateRef inDateRef, BSDTimeStructCopy *outBSDDate );

		static bool								ConvertBSDTimeToCFDate( struct tm *inBSDDate, CFDateRef *outDateRef );
        static bool								ConvertBSDTimeToCFDate( BSDTimeStructCopy *inBSDDate, CFDateRef *outDateRef );
		
	protected:
		
		virtual int								ConvertPropertyListPolicyToStruct( CFMutableDictionaryRef inPolicyDict ) = 0;
		virtual int								ConvertStructToPropertyListPolicy( void ) = 0;
		virtual bool							GetBooleanForKey( CFStringRef inKey, bool *outValue );
		
		CFMutableDictionaryRef mPolicyDict;
};


#endif



