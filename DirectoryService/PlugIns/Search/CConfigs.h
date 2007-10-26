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
 * @header CConfigs
 */

#ifndef __CConfigs_h__
#define __CConfigs_h__	1

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>		//for CF classes and property lists - XML config data

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
#include "PrivateTypes.h"

#define AUGMENT_RECORDS 1

//used for fSearchPolicy
const UInt32	kAutomaticSearchPolicy	= 1;
const UInt32	kLocalSearchPolicy		= 2;
const UInt32	kCustomSearchPolicy		= 3;

//XML label tags
#define	kXMLSearchPathVersionKey		"Search Node PlugIn Version"
#define kXMLSearchPolicyKey				"Search Policy"
#define kXMLSearchPathArrayKey			"Search Node Custom Path Array"
#define kXMLSearchDHCPLDAP				"DHCP LDAP"

#if AUGMENT_RECORDS
#define kXMLAugmentSearchKey			"Augment Search"				//bool deciding whether to attempt to augment search data
#define kXMLAugmentDirNodeNameKey		"Augment Directory Node Name"	//nodename on which to search for augment record data
#define kXMLToBeAugmentedDirNodeNameKey	"Augmented Directory Node Name"	//nodename which is augmented with data from another node
#define kXMLAugmentAttrListDictKey		"Augment Attribute List"		//list of specific augment attributes per record type
//dictionary of record type keys pointing to arrays of strings holding attribute types
#endif

typedef struct sSearchList {
	bool				fOpened;
	bool				fHasNeverOpened;
	bool				fNodeReachable;
	tDirNodeReference	fNodeRef;
	char			   *fNodeName;
	tDataList		   *fDataList;
	sSearchList		   *fNext;
} sSearchList;

class CConfigs
{
public:
						CConfigs			( void );
	SInt32				Init				( const char *inSearchNodeConfigFilePrefix, UInt32 &outSearchPolicy );
	sSearchList		   *GetCustom  			( void );
	virtual			   ~CConfigs			( void );
	SInt32				CleanListData		( sSearchList *inList );
	SInt32				SetListArray		( CFMutableArrayRef inCSPArray );
	SInt32				WriteConfig			( void );
	SInt32				SetSearchPolicy		( UInt32 inSearchPolicy );
	void				SetDHCPLDAPDictionary ( CFDictionaryRef dhcpLDAPdict );
	CFDictionaryRef		GetDHCPLDAPDictionary ( void );
#if AUGMENT_RECORDS
	CFDictionaryRef		AugmentAttrListDict ( void );
	char			   *AugmentDirNodeName	( void );
	char			   *ToBeAugmentedDirNodeName( void );
	bool				AugmentSearch		( void );
	void				UpdateAugmentDict	( CFDictionaryRef inDict );
#endif
	bool				IsDHCPLDAPEnabled	( void );

protected:
	SInt32				ConfigList			( void );
	SInt32				ConfigSearchPolicy 	( void );
	char			   *GetVersion			( CFDictionaryRef configDict );
#if AUGMENT_RECORDS
	char			   *GetAugmentDirNodeName ( CFDictionaryRef configDict );
	char			   *GetToBeAugmentedDirNodeName ( CFDictionaryRef configDict );
	bool				GetAugmentSearch	( CFDictionaryRef configDict );
	CFDictionaryRef		GetAugmentAttrListDict ( CFDictionaryRef configDict );
#endif
	UInt32				GetSearchPolicy		( CFDictionaryRef configDict );
	CFArrayRef			GetListArray		( CFDictionaryRef configDict );
	sSearchList		   *MakeListData		( char *inNodeName );

private:
	sSearchList	   *pSearchNodeList;
	UInt32			fSearchNodeListLength;
	UInt32			fSearchPolicy;
	tDirReference	fDirRef;
	CFMutableDictionaryRef	fConfigDict;
	char		   *fSearchNodeConfigFileName;
	char		   *fSearchNodeConfigBackupFileName;
	char		   *fSearchNodeConfigCorruptedFileName;
	CFStringRef		fXMLSearchPathVersionKeyString;
	CFStringRef		fXMLSearchPolicyKeyString;
	CFStringRef		fXMLSearchPathArrayKeyString;
	CFStringRef		fXMLSearchDHCPLDAPString;
#if AUGMENT_RECORDS
	bool			bAugmentSearch;
	CFStringRef		fXMLAugmentSearchKeyString;
	CFStringRef		fXMLToBeAugmentedDirNodeNameKeyString;
	char		   *fAugmentDirNodeName;
	char		   *fToBeAugmentedDirNodeName;
	CFStringRef		fXMLAugmentDirNodeNameKeyString;
	CFStringRef		fXMLAugmentAttrListDictKeyString;
	CFDictionaryRef	fAugmentAttrListDict;
#endif
};

#endif	// __CConfigs_h__
