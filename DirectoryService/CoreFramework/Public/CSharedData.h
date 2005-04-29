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
 * @header CSharedData
 */

#ifndef __CSharedData_h__
#define	__CSharedData_h__	1

#include <DirectoryServiceCore/PrivateTypes.h>

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
// LDAPv2 Plugin Request Codes
	eDSCustomCallLDAPv2ReadConfigSize				= 66,
	eDSCustomCallLDAPv2ReadConfigData				= 77,
	eDSCustomCallLDAPv2WriteConfigData				= 88,
	eDSCustomCallLDAPv2Reinitialize					= 99,
	
// LDAPv3 Plugin Request Codes
	eDSCustomCallLDAPv3ReadConfigSize				= 66,
	eDSCustomCallLDAPv3ReadConfigData				= 77,
	eDSCustomCallLDAPv3WriteConfigData				= 88,
	
// Search Plugin Request Codes
	eDSCustomCallSearchSetPolicyAutomatic			= 111,
	eDSCustomCallSearchSetPolicyLocalOnly			= 222,
	eDSCustomCallSearchSetPolicyCustom				= 333,
	eDSCustomCallSearchSetCustomNodeList			= 444,
	eDSCustomCallSearchReadDHCPLDAPSize				= 555,
	eDSCustomCallSearchReadDHCPLDAPData				= 556,
	eDSCustomCallSearchWriteDHCPLDAPData			= 557,
	
// Configure Plugin Request Codes
	eDSCustomCallConfigureGetAuthRef				= 111,
	eDSCustomCallConfigureCheckAuthRef				= 223,
	eDSCustomCallConfigureDestroyAuthRef			= 333,
	eDSCustomCallConfigureSCGetKeyPathValueSize		= 444,
	eDSCustomCallConfigureSCGetKeyPathValueData		= 445,
	eDSCustomCallConfigureSCGetKeyValueSize			= 446,
	eDSCustomCallConfigureSCGetKeyValueData			= 447,
	eDSCustomCallConfigureWriteSCConfigData			= 555,
	eDSCustomCallActivatePerfMonitor				= 666,
	eDSCustomCallDeactivatePerfMonitor				= 667,
	eDSCustomCallDumpStatsPerfMonitor				= 668,
	eDSCustomCallFlushStatsPerfMonitor				= 669,
	eDSCustomCallTogglePlugInStateBase				= 1000
} tPluginCustomCallRequestCode;

#ifdef __cplusplus
class CShared
{
public:
	static	void		LogIt				( uInt32 inMsgType, const char *inFmt, ... );
};
#endif

#endif // __CSharedData_h__

