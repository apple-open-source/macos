/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 
/*!
 *  @header CNSLResult
 *
 *	This class represents a result of a single service.  This is composed of at least
 *	a url, as well as an optional list of attribute value pairs.
 *
 *	All values are assumed to be UTF8 encoded
 */

#ifndef _CNSLResult_
#define _CNSLResult_ 1

#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryService/DirServicesConst.h>

#include "CNSLPlugin.h"

#define kNSLAttrServiceType			kDS1AttrServiceType
#define kNSLAttrRecordType			kDSNAttrRecordType

CFStringRef SLPRAdminNotifierCopyDesctriptionCallback ( const void *item );
Boolean SLPRAdminNotifierEqualCallback ( const void *item1, const void *item2 );

class CNSLResult
{
public:
                                    CNSLResult			();
                                    CNSLResult			( CFMutableDictionaryRef initialResults );
                                    ~CNSLResult			();
                                    
            void					SetURL				( const char* urlPtr );
            void					SetURL				( CFStringRef urlStringRef );
            
            void					SetServiceType		( const char* serviceType );
            void					SetServiceType		( CFStringRef serviceTypeRef );
            
            void					AddAttribute		( const char* key, const char* value );
            void					AddAttribute		( CFStringRef keyRef, CFStringRef valueRef );
            
            CFStringRef				GetURLRef			( void );
            CFStringRef				GetServiceTypeRef	( void );
            CFStringRef				GetAttributeRef		( CFStringRef keyRef ) const;
            
            CFDictionaryRef			GetAttributeDict	( void ) const { return mAttributes; }
protected:

private:
            CFMutableDictionaryRef	mAttributes;
			CNSLResult*				mSelfPtr;
};


#endif	// #ifndef
