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

//used for fSearchPolicy
const uInt32	kNetInfoSearchPolicy	= 1;
const uInt32	kLocalSearchPolicy		= 2;
const uInt32	kCustomSearchPolicy		= 3;

//XML label tags
#define	kXMLSearchPathVersionKey		"Search Node PlugIn Version"
#define kXMLSearchPolicyKey				"Search Policy"
#define kXMLSearchPathArrayKey			"Search Node Custom Path Array"
#define kXMLSearchDHCPLDAP				"DHCP LDAP"

typedef struct sSearchList {
	bool				fOpened;
	tDirNodeReference	fNodeRef;
	char			   *fNodeName;
	tDataList		   *fDataList;
	sSearchList		   *fNext;
} sSearchList;

class CConfigs
{
public:
						CConfigs			( void );
	sInt32				Init				( const char *inSearchNodeConfigFilePrefix, uInt32 &outSearchPolicy );
	sSearchList		   *GetCustom  			( void );
	virtual			   ~CConfigs			( void );
	sInt32				CleanListData		( sSearchList *inList );
	sInt32				SetListArray		( CFMutableArrayRef inCSPArray );
	sInt32				WriteConfig			( void );
	sInt32				SetSearchPolicy		( uInt32 inSearchPolicy );
	void				SetDHCPLDAPDictionary ( CFDictionaryRef dhcpLDAPdict );
	CFDictionaryRef		GetDHCPLDAPDictionary ( void );
	bool				IsDHCPLDAPEnabled	( void );

protected:
	sInt32				ConfigList			( void );
	sInt32				ConfigSearchPolicy 	( void );
	char			   *GetVersion			( CFDictionaryRef configDict );
	uInt32				GetSearchPolicy		( CFDictionaryRef configDict );
	CFArrayRef			GetListArray		( CFDictionaryRef configDict );
	sSearchList		   *MakeListData		( char *inNodeName );

private:
		sSearchList	   *pSearchNodeList;
		uInt32			fSearchNodeListLength;
		uInt32			fSearchPolicy;
		tDirReference	fDirRef;
		CFMutableDictionaryRef	fConfigDict;
		char		   *fSearchNodeConfigFileName;
		char		   *fSearchNodeConfigBackupFileName;
		char		   *fSearchNodeConfigCorruptedFileName;

};

#endif	// __CConfigs_h__
