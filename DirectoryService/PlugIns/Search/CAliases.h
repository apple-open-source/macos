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
 * @header CAliases
 */

#ifndef __CAliases_H__
#define __CAliases_H__	1

#include "PrivateTypes.h"
#include "DirServicesTypes.h"

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

class CAliases {
public:
typedef enum {
	errNoError			= 0,
	errInvalidXMLData	= -10128,
	errItemNotFound		= -10129,
	errInvalidDataType	= -10130,
	errEmptyArray		= -10131
} eAliasErrs;

				CAliases			( void );
	virtual	   ~CAliases			( void );

	sInt32		Initialize			( void *inXMLData, uInt32 inXMLDataLen );
	sInt32		GetRecordID			( char **outRecID );
	sInt32		GetRecordName		( tDataList *outDataList );
	sInt32		GetRecordType		( char **outRecType );
	sInt32		GetRecordLocation	( tDataList *outDataList );
	sInt32		GetAliasVersion		( char **outAliasVersion );

private:
	CFDataRef			fDataRef;
	CFPropertyListRef	fPlistRef;
	CFDictionaryRef		fDictRef;
	CFStringRef			fRecordIDString;
	CFStringRef			fRecordNameString;
	CFStringRef			fRecordTypeString;
	CFStringRef			fRecordLocationString;
	CFStringRef			fAliasVersionString;

};

#endif	// __CAliases_H__
