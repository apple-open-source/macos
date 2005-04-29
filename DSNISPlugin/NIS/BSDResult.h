/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *  @header BSDResult
 *
 *	This class represents a result of a single service.  This is composed of at least
 *	a url, as well as an optional list of attribute value pairs.
 *
 *	All values are assumed to be UTF8 encoded
 */

#ifndef _BSDResult_
#define _BSDResult_ 1

#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryService/DirServicesConst.h>

#define kNSLAttrServiceType			kDS1AttrServiceType
#define kNSLAttrRecordType			kDSNAttrRecordType

class BSDResult
{
public:
                                    BSDResult			();
                                    BSDResult			( CFMutableDictionaryRef initialResults );
                                    ~BSDResult			();
                                    
            void					AddAttribute		( const char* key, const char* value );
            void					AddAttribute		( CFStringRef keyRef, CFStringRef valueRef );
            
            CFStringRef				GetAttributeRef		( CFStringRef keyRef ) const;
            
            CFDictionaryRef			GetAttributeDict	( void ) const { return mAttributes; }
protected:

private:
            CFMutableDictionaryRef	mAttributes;
			BSDResult*				mSelfPtr;
};


#endif	// #ifndef
