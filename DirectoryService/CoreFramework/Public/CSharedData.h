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
 * @header CSharedData
 */

#ifndef __CSharedData_h__
#define	__CSharedData_h__	1

#include "DirServicesTypes.h"
#include "PrivateTypes.h"

#define		kstrDefaultLocalNodeName			"/NetInfo/DefaultLocalNode"
#define		kstrAuthenticationNodeName			"/Search"
#define		kstrContactsNodeName				"/Search/Contacts"
#define		kstrNetworkNodeName					"/Search/Network"
#define		kstrAuthenticationConfigFilePrefix	"SearchNodeConfig"
#define		kstrContactsConfigFilePrefix		"ContactsNodeConfig"

/*!
 * @defined kDSNAttrDefaultLDAPPaths
 * @discussion Represents the list of default LDAP paths used by the search node.
 *     Typically this list will be initially obtained from the host DHCP server
 */
#define		kDSNAttrDefaultLDAPPaths		"dsAttrTypeStandard:DefaultLDAPPaths"

typedef enum {
	keNullMetaType	= 0x00000000,
	keNodeLocation	= 0x00000001,
	keTargetAlias	= 0x00000002,
	keSourceAlias	= 0x00000004
} eMetaTypes;

typedef enum {
	keUnknown		= 0,
	keNormal		= 1,
	keAliases		= 2
} eBufferType;


class CShared
{
public:
	static	void		LogIt				( uInt32 inMsgType, const char *inFmt, ... );
};

#endif // __CSharedData_h__

