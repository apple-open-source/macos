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

#ifndef __CPOLICYGLOBALXML__
#define __CPOLICYGLOBALXML__

#include <PasswordServer/CPolicyBase.h>

class CPolicyGlobalXML : public CPolicyBase
{
	public:
	
												CPolicyGlobalXML();
												CPolicyGlobalXML( CFDictionaryRef inPolicyDict );
												CPolicyGlobalXML( const char *xmlDataStr );
		virtual									~CPolicyGlobalXML();
		
		virtual void							CPolicyCommonInit( void );
		virtual void							GetPolicy( PWGlobalAccessFeatures *outPolicy );
		virtual char *							GetPolicyAsSpaceDelimitedData( void );
		virtual void							SetPolicy( PWGlobalAccessFeatures *inPolicy );
		virtual void							SetPolicyExtra( PWGlobalAccessFeatures *inPolicy, PWGlobalMoreAccessFeatures *inMorePolicy );
		
	protected:
		
		virtual int								ConvertPropertyListPolicyToStruct( CFMutableDictionaryRef inPolicyDict );
		virtual int								ConvertStructToPropertyListPolicy( void );

		PWGlobalAccessFeatures mGlobalPolicy;
		PWGlobalMoreAccessFeatures mExtraGlobalPolicy;
};


#endif


