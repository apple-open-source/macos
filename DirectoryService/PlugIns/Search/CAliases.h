/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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

#define	kRecordID		"dsRecordID"
#define	kRecordName		"dsRecordName"
#define	kRecordType		"dsRecordType"
#define	kRecordLocation	"dsRecordLocation"
#define	kAliasVersion	"dsAliasVersion"


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
};

#endif	// __CAliases_H__
